struct vma{
  uint64 addr;
  int length;
  int permissions;
  struct file* fp;
  int ref;
  int offset;
  int prot;
  int flags;
  int removed_pages;
  struct vma *next;
};