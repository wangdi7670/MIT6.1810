struct vma{
  uint64 addr;
  int length;
  int permissions;
  struct file* fp;
  int ref;             // 1 indicates that there is a process using it, 0 indicates that no process
  int offset;
  int prot;
  int flags;
  int removed_pages;
  struct vma *next;
};