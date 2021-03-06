#define _POSIX_C_SOURCE 200809L
#include <stdc.h>
#include <edit.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

/******************************************************************************/

static void* emalloc(size_t sz) {
    void* ptr = malloc(sz);
    if (NULL == ptr) {
        perror("malloc() :");
        exit(1);
    }
    return ptr;
}

static size_t pagealign(size_t sz) {
    size_t pgsize = sysconf(_SC_PAGE_SIZE), alignmask = pgsize - 1;
    if (sz & alignmask)
        sz += pgsize - (sz & alignmask);
    return sz;
}

static Sel selconvert(Buf* buf, Sel* sel) {
    if (!sel) sel = &(buf->selection);
    if (sel->end < sel->beg)
        return (Sel){ .beg = sel->end, .end = sel->beg, .col = sel->col };
    else
        return (Sel){ .beg = sel->beg, .end = sel->end, .col = sel->col };
}

static void selupdate(Buf* buf, Sel* dest, Sel* src) {
    if (!dest) dest = &(buf->selection);
    if (dest->end < dest->beg)
        dest->beg = src->end, dest->end = src->beg, dest->col = src->col;
    else
        dest->beg = src->beg, dest->end = src->end, dest->col = src->col;
}

static char getb(Buf* buf, Sel* sel) {
    char* ptr = buf->bufstart + sel->end;
    if (ptr >= buf->gapstart)
        ptr += (buf->gapend - buf->gapstart);
    return *ptr;
}

static void syncgap(Buf* buf, Sel* sel) {
}

/******************************************************************************/

void buf_init(Buf* buf, void (*errfn)(char*)) {
    /* cleanup old data if there is any */
    if (buf->bufstart) {
        free(buf->bufstart);
        buf->bufstart = NULL;
    }
    /* reset the state to defaults */
    memset(buf, 0, sizeof(Buf));
    buf->expand_tabs = true;
    buf->bufsize     = pagealign(1);
    buf->bufstart    = emalloc(buf->bufsize);
    buf->bufend      = buf->bufstart + buf->bufsize;
    buf->gapstart    = buf->bufstart;
    buf->gapend      = buf->bufend;
    buf->errfn       = errfn;
}

void buf_load(Buf* buf, char* path) {
    /* process the file path and address */
    if (!path) return;
    if (path[0] == '.' && path[1] == '/')
        path += 2;
    buf->path = strdup(path);
    char* addr = strrchr(buf->path, ':');
    if (addr) *addr = '\0', addr++;
    /* load the contents from the file */
    int fd, nread;
    struct stat sb = {0};
    if (((fd = open(path, O_RDONLY, 0)) >= 0) && (fstat(fd, &sb) >= 0) && (sb.st_size > 0)) {
        /* allocate the buffer in advance */
        free(buf->bufstart);
        buf->bufsize  = pagealign(sb.st_size);
        buf->bufstart = malloc(buf->bufsize);
        buf->bufend   = buf->bufstart + buf->bufsize;
        buf->gapstart = buf->bufstart;
        buf->gapend   = buf->bufend;
        /* Read the file into the buffer */
        while (sb.st_size && (nread = read(fd, buf->gapstart, sb.st_size)) > 0)
            buf->gapstart += nread, sb.st_size -= nread;
        if (nread < 0) buf->errfn("Failed to read file");
    }
    if (fd > 0) close(fd);
}

void buf_reload(Buf* buf) {
    void (*errfn)(char*) = buf->errfn;
    char* path = buf->path;
    buf_init(buf, errfn);
    buf_load(buf, path);
}

void buf_save(Buf* buf) {
    char* wptr;
    long fd, nwrite, towrite;
    if (buf->path && (fd = open(buf->path, O_WRONLY|O_CREAT, 0644)) >= 0) {
        /* write the chunk before the gap */
        wptr = buf->bufstart, towrite = (buf->gapstart - buf->bufstart);
        while (towrite && ((nwrite = write(fd, wptr, towrite)) > 0))
            wptr += nwrite, towrite -= nwrite;
        /* write the chunk after the gap */
        wptr = buf->gapend, towrite = (buf->bufend - buf->gapend);
        while (towrite && ((nwrite = write(fd, wptr, towrite)) > 0))
            wptr += nwrite, towrite -= nwrite;
        close(fd);
        /* report success or failure */
        if (nwrite >= 0)
            buf->modified = false;
        else
            buf->errfn("Failed to write file");
    }
}

int buf_getc(Buf* buf, Sel* sel) {
    Sel lsel = selconvert(buf, sel);
    return getb(buf, &lsel);
}

void buf_putc(Buf* buf, Sel* sel, int rune, int fmtopts) {
    Sel lsel = selconvert(buf, sel);
    selupdate(buf, sel, &lsel);
}

void buf_puts(Buf* buf, Sel* sel, char* str, int fmtopts) {
    Sel lsel = selconvert(buf, sel);
    syncgap(buf, &lsel);
    selupdate(buf, sel, &lsel);
}

void buf_del(Buf* buf, Sel* sel) {
    Sel lsel = selconvert(buf, sel);
    syncgap(buf, &lsel);
    selupdate(buf, sel, &lsel);
}

size_t buf_size(Buf* buf) {
    return (buf->bufend - buf->bufstart) - (buf->gapend - buf->gapstart);
}

