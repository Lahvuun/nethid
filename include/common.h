#include <stdint.h>
#include <sys/types.h>

enum message_type {
	MESSAGE_GET_REPORT_DESCRIPTOR = 0,
	MESSAGE_GET_RAW_INFO,
	MESSAGE_GET_RAW_NAME,
	MESSAGE_GET_FEATURE_REPORT,
	MESSAGE_OUTPUT,
	MESSAGE_GET_QUEUED_REPORT,
};

struct message_header {
	enum message_type type;
	uint32_t length;
};

int log_error(const char *const func, const char *const file, const int line);
int log_error_errno(const char *const func, const char *const file, const int line);

size_t resize_buffer(uint8_t **buf, size_t length);

ssize_t read_all(int fd, void *buf, size_t count);
ssize_t write_all(int fd, const void *buf, size_t count);
