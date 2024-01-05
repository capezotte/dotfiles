#include <pulse/pulseaudio.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef WOB
#define WOB 1
#endif

#if WOB
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <signal.h>
static int wobfd = -1;
#endif

static struct {
	unsigned int volume;
	signed char mute;
} prev = { UINT_MAX, 2 };
static const char *default_sink;
static pa_mainloop *pam;
static pa_context  *pac;

/* Prints volume (callback for get_sink_info) */
static void print_volume(pa_context *c, const pa_sink_info *i, int is_last, void *userdata) {
#if WOB
	/* open wobfifo */
	char buf[PATH_MAX];
	char *runtimedir = getenv("XDG_RUNTIME_DIR");
	if (!runtimedir) runtimedir = ".";
	snprintf(buf, PATH_MAX, "%s/%s", runtimedir, "wob.fifo");
	wobfd = wobfd < 0 ? open(buf, O_WRONLY | O_NONBLOCK) : wobfd;
#endif

	if (i == NULL)
		return;

	unsigned int volume = pa_cvolume_avg(&i->volume);
	int mute = i->mute || pa_cvolume_is_muted(&i->volume);

	if (prev.volume == volume && prev.mute == mute)
		return;

	prev.volume = volume; prev.mute = mute;

	// convert to perc prior to printing
	volume *= 153;
	volume /= PA_VOLUME_UI_MAX;

	if (mute) {
		puts("mute");
	} else {
		printf("%d%%\n", volume);
	}
#	if WOB
	if (wobfd >= 0) {
		dprintf(wobfd, "%d %s\n", volume, mute ? "mute" : "snd");
	}
#	endif
}

/* Update default sink information */
static void get_default_sink(pa_context *c, const pa_server_info *i, void *userdata) {
	default_sink = i->default_sink_name;
}
static inline void update_default_sink() {
	pa_operation_unref(pa_context_get_server_info(pac, get_default_sink, NULL));
	pa_operation_unref(pa_context_get_sink_info_by_name(pac, default_sink, print_volume, NULL));
}

/* Subscriber callback */
static void change_detect(pa_context *c, pa_subscription_event_type_t t, uint32_t idx, void *userdata) {
	if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE) {
		update_default_sink();
	}
}

/* React when ready */
static void react_ready(pa_context *c, void *userdata) {
	if (pa_context_get_state(c) == PA_CONTEXT_READY) {
		pa_operation_unref(pa_context_subscribe(c, PA_SUBSCRIPTION_MASK_SINK, NULL, NULL));
		update_default_sink();
	}
}

#if WOB
void sigpipe_handle(int sigpipe) {
	if (wobfd >= 0) {
		close(wobfd);
	} else {
		exit(1);
	};
	wobfd = -1;
}
#endif

/* Finally */
int main() {
	setlinebuf(stdout);
#if WOB
	/* ignore broken wob pipe */
	struct sigaction act;
	act.sa_handler = sigpipe_handle;
	act.sa_flags = 0;
	sigaction(SIGPIPE, &act, NULL);
#endif
	pam = pa_mainloop_new();
	if (pam == NULL) {
		fputs("Can't mainloop.\n", stderr);
		return 1;
	}
	pac = pa_context_new(pa_mainloop_get_api(pam), NULL);
	if (pac == NULL) {
		fputs("No context.\n", stderr);
		return 1;
	}
	/* Set callbacks and connect */
	/* PROTIP: callbacks are needed because doing stuff before callback says it's ready gives BAD STATE */
	pa_context_set_state_callback(pac, react_ready, NULL);
	pa_context_set_subscribe_callback(pac, change_detect, NULL);
	if (pa_context_connect(pac, NULL, 0, NULL) < 0) {
		fprintf(stderr, "No connect: %s\n", pa_strerror(pa_context_errno(pac)));
		return 1;
	}
	int r;
	pa_mainloop_run(pam, &r);
	return r;
}
