#include <linux/kernel.h>
#include <linux/module.h>

static int __init gomi210_hello_module_init(void)
{
  printk("Hello, GoMi210 module is installed !\n");   
    
  return 0;
}

static void __exit gomi210_hello_module_cleanup(void)
{
  printk("Good-bye, GoMi210 module was removed!\n");
}

module_init(gomi210_hello_module_init);
module_exit(gomi210_hello_module_cleanup);
MODULE_LICENSE("GPL");
