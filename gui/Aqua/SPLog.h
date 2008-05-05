/* vim: ft=objc
 */
#import <Foundation/Foundation.h>

void SPLogMessage(NSString *fmt, ...);

#define SPLog(fmt, ...)  SPLogMessage(@"[%d] (%s:%i) " fmt, getpid(), __func__, __LINE__, ## __VA_ARGS__)

