
#ifndef __GST_COMPAT_H__
#define __GST_COMPAT_H__

/**
 * GST_CLOCK_STIME_NONE: (value -9223372036854775808) (type GstClockTimeDiff)
 *
 * Constant to define an undefined clock time.
 */
#define GST_CLOCK_STIME_NONE             G_MININT64
/**
 * GST_CLOCK_STIME_IS_VALID:
 * @time: signed clock time to validate
 *
 * Tests if a given #GstClockTimeDiff of #gint64 represents a valid defined time.
 *
 * Since: 1.6
 */
#define GST_CLOCK_STIME_IS_VALID(time)   (((GstClockTimeDiff)(time)) != GST_CLOCK_STIME_NONE)

/**
 * GST_STIME_FORMAT:
 *
 * A string that can be used in printf-like format strings to display a signed
 * #GstClockTimeDiff or #gint64 value in h:m:s format.  Use GST_TIME_ARGS() to
 * construct the matching arguments.
 *
 * Example:
 * |[
 * printf("%" GST_STIME_FORMAT "\n", GST_STIME_ARGS(ts));
 * ]|
 *
 * Since: 1.6
 */
#define GST_STIME_FORMAT "c%" GST_TIME_FORMAT
/**
 * GST_STIME_ARGS:
 * @t: a #GstClockTimeDiff or #gint64
 *
 * Format @t for the #GST_STIME_FORMAT format string. Note: @t will be
 * evaluated more than once.
 *
 * Since: 1.6
 */
#define GST_STIME_ARGS(t)						\
  ((t) == GST_CLOCK_STIME_NONE || (t) >= 0) ? '+' : '-',		\
    GST_CLOCK_STIME_IS_VALID (t) ?					\
    (guint) (((GstClockTime)(ABS(t))) / (GST_SECOND * 60 * 60)) : 99,	\
    GST_CLOCK_STIME_IS_VALID (t) ?					\
    (guint) ((((GstClockTime)(ABS(t))) / (GST_SECOND * 60)) % 60) : 99,	\
    GST_CLOCK_STIME_IS_VALID (t) ?					\
    (guint) ((((GstClockTime)(ABS(t))) / GST_SECOND) % 60) : 99,	\
    GST_CLOCK_STIME_IS_VALID (t) ?					\
    (guint) (((GstClockTime)(ABS(t))) % GST_SECOND) : 999999999

#endif /* __GST_COMPAT_H__ */
