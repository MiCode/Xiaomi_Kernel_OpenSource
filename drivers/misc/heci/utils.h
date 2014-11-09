#ifndef UTILS__H
 #define UTILS__H

#define	WAIT_FOR_SEND_SLICE	(HZ / 10)
#define	WAIT_FOR_CONNECT_SLICE	(HZ / 10)

/*
 * Waits for specified event when a thread that triggers event can't signal
 * Also, waits *at_least* `timeinc` after condition is satisfied
 */
#define	timed_wait_for(timeinc, condition) \
	do { \
		int completed = 0; \
		do { \
			unsigned long	j; \
			int	done = 0; \
\
			completed = (condition); \
			for (j = jiffies, done = 0; !done; ) { \
				schedule_timeout(timeinc); \
				if (jiffies >= j + timeinc) \
					done = 1; \
			} \
		} while (!(completed)); \
	} while (0)


/*
 * Waits for specified event when a thread that triggers event can't signal with timeout (use whenever we may hang)
 */
#define	timed_wait_for_timeout(timeinc, condition, timeout) \
	do { \
		int	t = timeout; \
		do { \
			unsigned long	j; \
			int	done = 0; \
\
			for (j = jiffies, done = 0; !done; ) { \
				schedule_timeout(timeinc); \
				if (jiffies >= j + timeinc) \
					done = 1; \
			} \
			t -= timeinc; \
			if (t <= 0) \
				break; \
		} while (!(condition)); \
	} while (0)

#endif /* UTILS__H */

