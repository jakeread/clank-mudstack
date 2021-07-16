#ifndef CONFIG_H_ 
#define CONFIG_H_

//#define UCBUS_IS_HEAD
#define UCBUS_IS_DROP

// if you're using the 'module board' https://gitlab.cba.mit.edu/jakeread/ucbus-module
// the first (og) revision has an SMT header, and some of the RS485 pins are varied, 
// set this flag. otherwise, if you have thru-hole JTAG header, comment it out 
// #define IS_OG_MODULE

#endif 