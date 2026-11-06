#ifndef LEXXHARD_MICROROS_SESSION_H_
#define LEXXHARD_MICROROS_SESSION_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Thread entry points of the two micro-ROS sessions (unit h); they never return. */
void lexxhard_microros_session_uart6(void);
void lexxhard_microros_session_uart2(void);

#ifdef __cplusplus
}
#endif

#endif /* LEXXHARD_MICROROS_SESSION_H_ */
