#ifndef __LINH_DEBUG_H__
#define __LINH_DEBUG_H__

#define __DEBUG 1
#define __DBG_INFO 1
#if __DEBUG
#define print_dbg(fmt,args...) pr_info("LINH rmi4 %s(): DBG " fmt "\n", __func__, ##args)
#else
#define print_dbg(fmt,args...)
#endif /* __DEBUG */
#if __DBG_INFO
#define print_inf(fmt,args...) pr_info("LINH rmi4 %s(): INF " fmt "\n", __func__, ##args)
#else
#define print_inf(fmt,args...)
#endif
#define print_infa(fmt,args...) pr_info("LINH rmi4 %s(): INF " fmt "\n", __func__, ##args)
#define print_warn(fmt,args...) pr_info("LINH rmi4 %s(): WARN " fmt "\n", __func__, ##args)
#define print_err(fmt,args...) pr_err("LINH rmi4 %s(): ERR " fmt "\n", __func__, ##args)

#endif /* __LINH_DEBUG_H__ */