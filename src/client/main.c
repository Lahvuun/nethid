#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <stdlib.h>
#include <linux/uhid.h>
#include <linux/hidraw.h>

#define FDIN 6
#define FDOUT 7

#include "common.h"

static_assert(sizeof(uint32_t) == 4, "uint32_t must be 4 bytes long");
static_assert(sizeof(size_t) >= sizeof(uint32_t), "size_t must fit uint32_t");

static uint8_t *in_buffer = NULL;
static size_t in_buffer_length = 0;

static ssize_t fetch_raw_info(struct uhid_create2_req *req) 
{
	struct message_header header = {
		.type = MESSAGE_GET_RAW_INFO,
		.length = 0,
	};
	ssize_t result = write_all(FDOUT, &header, sizeof(header));
	if (result < 1) {
		if (result < 0) {
			log_error("write_all", __FILE__, __LINE__);
		}
		return result;
	}

	// TODO: check for endianness???
	uint32_t payload_length = 0;
	result = read_all(FDIN, &payload_length, sizeof(payload_length));
	if (result < 1) {
		if (result < 0) {
			log_error("read_all", __FILE__, __LINE__);
		}
		return result;
	}
	if (payload_length != sizeof(struct hidraw_devinfo)) {
		fprintf(stderr, "unexpected hidraw_devinfo size\n");
		return -1;
	}

	struct hidraw_devinfo devinfo = {
		.bustype = 0,
		.vendor = 0,
		.product = 0,
	};
	result = read_all(FDIN, &devinfo, payload_length);
	if (result < 0) {
		log_error("read_all", __FILE__, __LINE__);
	}

	req->bus = devinfo.bustype;
	req->vendor = devinfo.vendor;
	req->product = devinfo.product;

	return result;
}

static ssize_t fetch_raw_name(struct uhid_create2_req *req, size_t name_length)
{
	struct message_header header = {
		.type = MESSAGE_GET_RAW_NAME,
		.length = 0,
	};
	ssize_t result = write_all(FDOUT, &header, sizeof(header));
	if (result < 1) {
		if (result < 0) {
			log_error("write_all", __FILE__, __LINE__);
		}
		return result;
	}

	uint32_t payload_length = 0;
	result = read_all(FDIN, &payload_length, sizeof(payload_length));
	if (result < 1) {
		if (result < 0) {
			log_error("read_all", __FILE__, __LINE__);
		}
		return result;
	}

	if (payload_length > name_length) {
		fprintf(stderr, "name doesn't fit, have %u, max %zu\n", payload_length, name_length);
		return -1;
	}

	result = read_all(FDIN, req->name, payload_length);
	if (result < 0) {
		log_error("read_all", __FILE__, __LINE__);
	}

	return result;
}

static ssize_t fetch_report_descriptor(struct uhid_create2_req *req, size_t buffer_length)
{
	struct message_header header = {
		.type = MESSAGE_GET_REPORT_DESCRIPTOR,
		.length = 0
	};
	ssize_t result = write_all(FDOUT, &header, sizeof(header));
	if (result < 1) {
		if (result < 0) {
			log_error("write_all", __FILE__, __LINE__);
		}
		return result;
	}

	// TODO: check for endianness???
	uint32_t payload_length = 0;
	result = read_all(FDIN, &payload_length, sizeof(payload_length));
	if (result < 1) {
		if (result < 0) {
			log_error("read_all", __FILE__, __LINE__);
		}
		return result;
	}

	if (buffer_length < payload_length) {
		fprintf(stderr, "rd_data can't fit report descriptor\n");
		return -1;
	}

	result = read_all(FDIN, req->rd_data, payload_length);
	if (result < 0) {
		log_error("read_all", __FILE__, __LINE__);
	}
	req->rd_size = payload_length;

	return result;
}

static ssize_t create_device(int fd)
{
	struct uhid_event event = {
		.type = UHID_CREATE2,
		.u.create2 = {
			.country = 0,
			.version = 1,

			.bus = BUS_USB,
			.vendor = 0x1337,
			.product = 0x1337,
		},
	};
	ssize_t result = fetch_raw_info(&event.u.create2);
	if (result < 1) {
		if (result < 0) {
			log_error("fetch_raw_info", __FILE__, __LINE__);
		}
		return result;
	}

	result = fetch_raw_name(&event.u.create2, sizeof(event.u.create2.name));
	if (result < 1) {
		if (result < 0) {
			log_error("fetch_raw_name", __FILE__, __LINE__);
		}
		return result;
	}

	result = fetch_report_descriptor(&event.u.create2, sizeof(event.u.create2.rd_data));
	if (result < 1) {
		if (result < 0) {
			log_error("fetch_report_descriptor", __FILE__, __LINE__);
		}
		return result;
	}

	result = write_all(fd, &event, sizeof(event));
	if (result < 0) {
		log_error("write_all", __FILE__, __LINE__);
		return result;
	}

#if 0
	if (in_buffer_length < payload_length) {
		in_buffer_length = resize_buffer(&in_buffer, payload_length);
		if (!in_buffer_length) {
			log_error("resize_buffer", __FILE__, __LINE__);
			return -1;
		}
	}
#endif

	return result;
}

static size_t handle_report_get_report(int fd, struct uhid_event *event)
{
	struct message_header header = {
		.type = MESSAGE_GET_FEATURE_REPORT,
		.length = sizeof(event->u.get_report.rnum),
	};
	ssize_t result = write_all(FDOUT, &header, sizeof(header));
	if (result < 1) {
		if (result < 0) {
			log_error("write_all", __FILE__, __LINE__);
		}
		return result;
	}

	result = write_all(FDOUT, &event->u.get_report.rnum, sizeof(event->u.get_report.rnum));
	if (result < 1) {
		if (result < 0) {
			log_error("write_all", __FILE__, __LINE__);
		}
		return result;
	}

	uint32_t payload_length = 0;
	result = read_all(FDIN, &payload_length, sizeof(payload_length));
	if (result < 1) {
		if (result < 0) {
			log_error("read_all", __FILE__, __LINE__);
		}
		return result;
	}

	if (sizeof(event->u.get_report_reply.data) < payload_length) {
		fprintf(stderr, "report reply data can't fit the whole payload\n");
		return -1;
	}

	// Probably not needed.
	result = read_all(FDIN, event->u.get_report_reply.data, payload_length);
	if (result < 1) {
		if (result < 0) {
			log_error("read_all", __FILE__, __LINE__);
		}
		return result;
	}
#if 0
	for (size_t i = 0; i < payload_length; ++i) {
		fprintf(stderr, "0x%x ", *(event->u.get_report_reply.data + i));
	}
	fprintf(stderr, "\n");
#endif
	event->u.get_report_reply.id = event->u.get_report.id;
	event->u.get_report_reply.err = 0;
	event->u.get_report_reply.size = payload_length;
	event->type = UHID_GET_REPORT_REPLY;
	result = write_all(fd, event, sizeof(struct uhid_event));
	if (result < 0) {
		log_error("write_all", __FILE__, __LINE__);
	}

	return result;
}

static ssize_t handle_report(int fd)
{
	struct uhid_event event;
	ssize_t result = read_all(fd, &event, sizeof(event));
	if (result < 1) {
		if (result < 0) {
			log_error("read_all", __FILE__, __LINE__);
		}
		return result;
	}

	switch (event.type) {
	case UHID_START:
	// TODO: "As long as you haven't received this event there is
	// actually no other process that reads your data so there is no need
	// to send UHID_INPUT2 events to the kernel."
	case UHID_OPEN:
		return result;
	case UHID_GET_REPORT:
		return handle_report_get_report(fd, &event);
	case UHID_OUTPUT:
		return 1;
	default:
		fprintf(stderr, "unknown event: %u\n", event.type);
		return -1;
	}

}

static ssize_t fetch_input(int fd)
{
	struct message_header header = {
		.type = MESSAGE_GET_QUEUED_REPORT,
		.length = 0,
	};
	ssize_t result = write_all(FDOUT, &header, sizeof(header));
	if (result < 1) {
		if (result < 0) {
			log_error("write_all", __FILE__, __LINE__);
		}
		return result;
	}

	uint32_t payload_length = 0;
	result = read_all(FDIN, &payload_length, sizeof(payload_length));
	if (result < 1) {
		if (result < 0) {
			log_error("read_all", __FILE__, __LINE__);
		}
		return result;
	}

	struct uhid_event event = {
		.type = UHID_INPUT2,
		.u.input2 = {
			.size = payload_length
		}
	};
	if (sizeof(event.u.input2.data) < payload_length) {
		fprintf(stderr, "data can't hold payload: need %u, can %zu\n", payload_length, sizeof(event.u.input2.data));
		return -1;
	}

	result = read_all(FDIN, event.u.input2.data, payload_length);
	if (result < 1) {
		if (result < 0) {
			log_error("read_all", __FILE__, __LINE__);
		}
		return result;
	}

	result = write_all(fd, &event, sizeof(event));
	if (result < 0) {
		log_error("write_all", __FILE__, __LINE__);
	}

	return result;
}

static ssize_t run_device(int fd)
{
	ssize_t result = create_device(fd);
	if (result < 1) {
		if (result < 0) {
			log_error("run_device", __FILE__, __LINE__);
		}
		return result;
	}

	struct pollfd fds[1]; 
	fds[0].fd = fd;
	fds[0].events = POLLIN;
	while (result > 0) {
		result = poll(fds, 1, 0);
		if (result < 0) {
			log_error_errno("poll", __FILE__, __LINE__);
			return -1;
		}
		if (fds[0].revents & POLLIN) {
			result = handle_report(fd);
		} else {
			result = fetch_input(fd);
		}
	}

	return result;
}

int main(void)
{
	bool success = true;
	int fd = open("/dev/uhid", O_RDWR);
	if (fd < 0) {
		log_error_errno("open", __FILE__, __LINE__);
		success = false;
		goto cleanup;
	}

	in_buffer = calloc(1024, sizeof(uint8_t));
	if (!in_buffer) {
		log_error_errno("calloc", __FILE__, __LINE__);
		success = false;
		goto cleanup_fd;
	}
	in_buffer_length = 1024;

	if (run_device(fd) < 0) {
		log_error("run_device", __FILE__, __LINE__);
		success = false;
	}

	free(in_buffer);

cleanup_fd:
	if (close(fd) < 0) {
		log_error_errno("close", __FILE__, __LINE__);
		success = false;
	}
cleanup:
	if (close(FDIN) < 0) {
		log_error_errno("close", __FILE__, __LINE__);
		success = false;
	}
	if (close(FDOUT) < 0) {
		log_error_errno("close", __FILE__, __LINE__);
		success = false;
	}

	if (success) {
		fprintf(stderr, "exiting normally\n");
	}
	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
