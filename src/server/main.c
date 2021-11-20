#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>
#include <linux/hidraw.h>
#include <linux/uhid.h>
#include <sys/ioctl.h>

#include "common.h"

#define BUFFER_LENGTH 1024

static_assert(sizeof(uint32_t) == 4, "uint32_t must be 4 bytes long");
static_assert(sizeof(__u32) == 4, "__u32 must be 4 bytes long");
static_assert(sizeof(size_t) >= sizeof(uint32_t), "size_t must fit uint32_t");

static uint8_t *in_buffer = NULL;
static size_t in_buffer_length = 0;

static ssize_t send_buf(void *buf, uint32_t length)
{
	ssize_t result = write_all(STDOUT_FILENO, &length, sizeof(length));
	if (result < 1) {
		if (result < 0) {
			log_error("write_all", __FILE__, __LINE__);
		}
		return result;
	}

	return write_all(STDOUT_FILENO, buf, length);
}

static ssize_t process_get_report_descriptor(int fd)
{
	__u32 desc_size = 0;
	if (ioctl(fd, HIDIOCGRDESCSIZE, &desc_size) < 0) {
		log_error_errno("ioctl", __FILE__, __LINE__);
		return -1;
	}

	struct hidraw_report_descriptor desc;
	// Normally I don't care about zeroing out, but there may be an extra
	// byte in the descriptor at the end. Kernel will ignore if set to 0,
	// so let's be safe it isn't.
	memset(&desc, '\0', sizeof(desc));
	desc.size = desc_size;

	if (ioctl(fd, HIDIOCGRDESC, &desc) < 0) {
		log_error_errno("ioctl", __FILE__, __LINE__);
		return -1;
	}

	return send_buf(desc.value, desc_size);
}

static ssize_t process_get_raw_info(int fd)
{
	struct hidraw_devinfo devinfo = {
		.bustype = 0x0,
		.product = 0x0,
		.vendor = 0x0,
	};
	if (ioctl(fd, HIDIOCGRAWINFO, &devinfo) < 0) {
		log_error_errno("ioctl", __FILE__, __LINE__);
		return -1;
	}

	return send_buf(&devinfo, sizeof(devinfo));
}

static ssize_t process_get_raw_name(int fd)
{
	// So, HIDIOCGRAWNAME apparently returns a UTF-8 string. I'm too lazy
	// to deal with that, so let's hope no vendor in the world actually
	// uses characters outside of ASCII (they probably do)
	char name[128] = "";
	if (ioctl(fd, HIDIOCGRAWNAME(128), name) < 0) {
		log_error_errno("ioctl", __FILE__, __LINE__);
		return -1;
	}

	return send_buf(name, sizeof(name));
}

static ssize_t process_get_feature_report(int fd, __u8 rnum)
{
	uint8_t buffer[UHID_DATA_MAX] = { 0 };
	buffer[0] = rnum;
	int result = ioctl(fd, HIDIOCGFEATURE(sizeof(buffer)), buffer);
	if (result < 0) {
		log_error_errno("ioctl", __FILE__, __LINE__);
		return -1;
	}
	return send_buf(buffer, result);
}

static ssize_t process_output(int fd, uint8_t *payload)
{
	// Unimplemented.
	return -1;
}

static ssize_t process_get_queued_report(int fd)
{
	struct pollfd fds[1];
	fds[0].fd = fd;
	fds[0].events = POLLIN;
	int poll_result = -1;
	ssize_t result = -1;
	// Exhaust the queue.
	do {
		poll_result = poll(fds, 1, 0);
		if (poll_result < 0) {
			log_error_errno("poll", __FILE__, __LINE__);
			return -1;
		}

		if (fds[0].revents & POLLIN) {
			// Assuming the kernel writes the whole report at
			// once.
			result = read(fd, in_buffer, in_buffer_length);
			if (result < 0) {
				log_error("read", __FILE__, __LINE__);
				return result;
			}
		} else if (poll_result) {
			// Unhandled event.
			log_error("poll", __FILE__, __LINE__);
			return -1;
		}
	} while (poll_result);

	// hidraw read() is not supposed to return 0 (I think).
	assert(result);
	if (result < 0) {
		result = read(fd, in_buffer, in_buffer_length);
		if (result < 0) {
			log_error("read", __FILE__, __LINE__);
			return result;
		}
	}

	if (result == in_buffer_length) {
		// A report longer than 1024 bytes, OK.
		log_error("read", __FILE__, __LINE__);
		return -1;
	}

	// I literally don't care.
	uint32_t payload_length = result;
	result = write_all(STDOUT_FILENO, &payload_length, sizeof(payload_length));
	if (result < 1) {
		if (result < 0) {
			log_error("write_all", __FILE__, __LINE__);
		}
		return result;
	}

	result = write_all(STDOUT_FILENO, in_buffer, payload_length);
	if (result < 0) {
		log_error("write_all", __FILE__, __LINE__);
	}

	return result;
}

static ssize_t answer_message(int fd, struct message_header *header, uint8_t *payload)
{
	switch (header->type) {
	case MESSAGE_GET_REPORT_DESCRIPTOR:
		return process_get_report_descriptor(fd);
	case MESSAGE_GET_RAW_INFO:
		return process_get_raw_info(fd);
	case MESSAGE_GET_RAW_NAME:
		return process_get_raw_name(fd);
	case MESSAGE_GET_FEATURE_REPORT:
		return process_get_feature_report(fd, payload[0]);
	case MESSAGE_OUTPUT:
		return process_output(fd, payload);
	case MESSAGE_GET_QUEUED_REPORT:
		return process_get_queued_report(fd);
	default:
		fprintf(stderr, "unknown message type\n");
		return -1;
	}
}

static ssize_t handle_message(int fd)
{
	struct message_header header = {
		.type = MESSAGE_GET_REPORT_DESCRIPTOR,
		.length = 0,
	};
	ssize_t result = read_all(STDIN_FILENO, &header, sizeof(header));
	if (result < 1) {
		if (result < 0) {
			fprintf(stderr, "read_all() failed\n");
		}
		return result;
	}

	if (in_buffer_length < header.length) {
		fprintf(stderr, "in_buffer_length: %zu, header.length: %x\n", in_buffer_length, header.length);
		in_buffer_length = resize_buffer(&in_buffer, header.length);
		if (!in_buffer_length) {
			return -1;
		}
	}

	result = read_all(STDIN_FILENO, in_buffer, header.length);
	if (result < 1) {
		if (result < 0) {
			fprintf(stderr, "read_all() failed\n");
			return result;
		}
		if (header.length) {
			return result;
		}
	}

	return answer_message(fd, &header, in_buffer);
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: nethidserver [PATH_TO_HIDRAW]\n");
		return EXIT_FAILURE;
	}

	int fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		log_error_errno("open", __FILE__, __LINE__);
		return EXIT_FAILURE;
	}

	ssize_t result = 0;
	in_buffer = calloc(BUFFER_LENGTH, sizeof(uint8_t));
	if (!in_buffer) {
		log_error_errno("calloc", __FILE__, __LINE__);
		result = -1;
		goto cleanup;
	}
	in_buffer_length = BUFFER_LENGTH;

	do {
		result = handle_message(fd);
	} while (result > 0);

	free(in_buffer);
cleanup:
	if (close(fd) < 0) {
		log_error_errno("close", __FILE__, __LINE__);
		return EXIT_FAILURE;
	}

	if (result < 0) {
		return EXIT_FAILURE;
	}

	fprintf(stderr, "exiting normally\n");
	return EXIT_SUCCESS;
}
