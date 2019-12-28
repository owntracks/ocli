#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <syslog.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include <libgen.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include <gps.h>
#include <libgen.h>
#include <mosquitto.h>
#include "json.h"
#include "utarray.h"

#define VERSION		0
#define TID		"L1"

#define MQTT_HOST	"127.0.0.1"
#define MQTT_PORT	1888
#define QOS0		0
#define QOS1		1
#define QOS2		2

#define max(a, b) ( (a > b) ? a : b )
#ifndef TRUE
# define TRUE (1)
#endif
#ifndef FALSE
# define FALSE (0)
#endif

#define PERIODIC_REPORT	NULL
#define ONDEMAND_REPORT "m"

/* Globals, configured from environment */
FILE *fixlog = NULL;		/* if open, record JSON to this file */
char *t_prefix = "owntracks/unix/cli";		/* topic prefix */

char *t_cmd, *t_report, *t_state;

UT_array *parms;
static struct mosquitto *mosq = NULL;
static struct gps_data_t gpsdata;
static int minmove = 00;	/* meters */
static int minsecs = 60;	/* seconds */

#define PROGNAME "owntracks-cli"
#define SIESTA	700		/* microseconds */
#define BAT0	"/sys/class/power_supply/BAT0/capacity"

void publish(char *topic, char *payload, int qos);
static void print_fix(struct gps_data_t *gpsdata, double time, char *reporttype);

struct udata {
	int mid;
};

long npubs = 0L;

static int batt_percent()
{
	int percent = -1;
	FILE *fp;

	if ((fp = fopen(BAT0, "r")) != NULL) {
		char buf[128];
		if (fgets(buf, sizeof(buf), fp) != NULL) {
			percent = atoi(buf);
		}
		fclose(fp);
	}
	return (percent);
}

static void fatal(void)
{
        if (mosq) {
                mosquitto_disconnect(mosq);
                mosquitto_loop_stop(mosq, FALSE);
                mosquitto_lib_cleanup();
        }
        exit(1);
}

void catcher(int sig)
{
	char info[BUFSIZ];

        sprintf(info, "Going down on signal %d", sig);
	publish(t_state, info, QOS1);	/* shouting in the dark ... */
	fatal();
}

void publish(char *topic, char *payload, int qos)
{
	int mid;
	int retain = FALSE;
	int rc;

        rc = mosquitto_publish(mosq, &mid, topic, strlen(payload), payload, qos, retain);
        if (rc != MOSQ_ERR_SUCCESS) {
                fprintf(stderr, "Cannot publish: %s\n", mosquitto_strerror(rc));
                fatal();
        }
}

void cb_pub(struct mosquitto *mosq, void *userdata, int pmid)
{
        // printf("cb_pub: mid = %d\n", pmid);
}

void cb_disconnect(struct mosquitto *mosq, void *userdata, int rc)
{
        if (rc == 0) {
                // Disconnect requested by client
        } else {
                fprintf(stderr, "%s: disconnected: reason: %d (%s)\n",
                        PROGNAME, rc, strerror(errno));
                fatal();
        }
}

static void print_internal()
{
	char *json_string;
	time_t now;
	JsonNode *jo;

	jo = json_mkobject();

	time(&now);
	json_append_member(jo, "_type", json_mkstring("internal"));
	json_append_member(jo, "tst", json_mknumber(now));
	json_append_member(jo, "npubs", json_mknumber(npubs));
	printf("minmove == %d\n", minmove);
	json_append_member(jo, "seconds", json_mknumber(minsecs));
	json_append_member(jo, "metres", json_mknumber(minmove));

	if ((json_string = json_stringify(jo, NULL)) != NULL) {
		publish(t_state, json_string, QOS1);

		free(json_string);
	}

	json_delete(jo);
}

void cb_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg)
{
	char info[BUFSIZ];
	char *tp = strrchr(msg->topic, '/');

	printf("topic == %s\n", msg->topic);

	if (!tp || !(*tp+1) || !(*tp+2))
		return;
	tp++;

	strcpy(info, "?");

	if (!strcmp(tp, "minmove")) {
		int n = atoi(msg->payload);
		minmove = (n >= 0) ? n : minmove;
		print_internal();
		return;
	} else if (!strcmp(tp, "minsecs")) {
		int n = atoi(msg->payload);
		minsecs = (n > 1) ? n : minsecs;
		print_internal();
		return;
	} else if (!strcmp(tp, "ping")) {
		strcpy(info, "PONG");
	} else if (!strcmp(tp, "report")) {
		print_fix(&gpsdata, 0, ONDEMAND_REPORT);
		return;
	} else if (!strcmp(tp, "info")) {
		print_internal();
		return;
	} else if (!strcmp(tp, "stop")) {
		catcher(0);
	}

	publish(t_state, info, QOS2);
}


#if 0
static JsonNode *double_str(char *fmt, double d)
{
	char buf[128];

	sprintf(buf, fmt, d);
	return json_mkstring(buf);
}
#endif

static void print_fix(struct gps_data_t *gpsdata, double ttime, char *reporttype)
{
	char buf[128], *json_string, **p;
	double accuracy;
	struct gps_fix_t *fix = &(gpsdata->fix);
	JsonNode *jo;
#if 1
	char tbuf[128];
#endif

	if (isnan(fix->latitude) ||
		isnan(fix->longitude) ||
		isnan(ttime)) {
		return;
	}

	if (!isnan(fix->epx) && !isnan(fix->epy)) {
		accuracy = max(fix->epx, fix->epy);
	} else if (isnan(fix->epx) && !isnan(fix->epy)) {
		accuracy = fix->epy;
	} else if (!isnan(fix->epx) && isnan(fix->epx)) {
		accuracy = fix->epx;
	} else {
		accuracy = 0.0;
	}

#if 1
	unix_to_iso8601(ttime, tbuf, sizeof(tbuf));
	printf("mode=%d, lat=%f, lon=%f, acc=%f, tst=%s (%f)\n",
		fix->mode,
		fix->latitude,
		fix->longitude,
		accuracy,
		tbuf,
		ttime);
#endif
	jo = json_mkobject();

	json_append_member(jo, "_type", json_mkstring("location"));
	json_append_member(jo, "v", json_mknumber(VERSION));
	json_append_member(jo, "lat", json_mknumber(fix->latitude));
	json_append_member(jo, "lon", json_mknumber(fix->longitude));
	json_append_member(jo, "acc", json_mknumber((long)accuracy));
	json_append_member(jo, "tst", json_mknumber(ttime));
	json_append_member(jo, "batt", json_mknumber(batt_percent()));
	json_append_member(jo, "tid", json_mkstring(TID));

	if (reporttype) {
		json_append_member(jo, "t", json_mkstring(reporttype));
	}

	/* DEBUGGING */
	sprintf(buf, "%d/%d", gpsdata->satellites_used, gpsdata->satellites_visible);
	json_append_member(jo, "sat", json_mkstring(buf));
	if ((fix->mode == MODE_3D) && !isnan(fix->altitude)) {
		json_append_member(jo, "alt", json_mknumber(fix->altitude));
	}
	if (!isnan(fix->speed)) {
		json_append_member(jo, "vel", json_mknumber(fix->speed));
	}

	/*
	 * For each of the filenames passed as parameters, use the basename
	 * as key for the string value of the first line in the file and add
	 * to the JSON.
	 */

	p = NULL;
	while ((p=(char**)utarray_next(parms, p))) {
		char *key = basename(*p), *val = NULL;
		FILE *fp;

		if ((fp = fopen(*p, "r")) != NULL) {
			char buf[1025], *bp;
			if (fgets(buf, sizeof(buf), fp) != NULL) {
				if ((bp = strchr(buf, '\r')) != NULL)
					*bp = 0;
				if ((bp = strchr(buf, '\n')) != NULL)
					*bp = 0;
				val = buf;
			}
			fclose(fp);
		}
		if (val) {
			json_append_member(jo, key, json_mkstring(val));
		} else {
			json_append_member(jo, key, json_mknull());
		}
	}

	if ((json_string = json_stringify(jo, NULL)) != NULL) {
		npubs++;
		publish(t_report, json_string, QOS1);

		if (fixlog) {
			fprintf(fixlog, "%s\n", json_string);
			fflush(fixlog);	/* FIXME */
		}
		free(json_string);
	}

	json_delete(jo);
}


static void conditionally_log_fix(struct gps_data_t *gpsdata)
{
	struct gps_fix_t *fix = &(gpsdata->fix);
	static double int_time, old_int_time;
	static double old_lat, old_lon;
	static bool first = true;

	int_time = fix->time;

	if ((int_time == old_int_time) || fix->mode < MODE_2D) {
		// puts("rubbish");
		usleep(SIESTA);
		return;
	}

	/* may not be worth logging if we've moved only a very short distance */
	if (minmove>0 && !first && earth_distance(
					fix->latitude,
					fix->longitude,
					old_lat, old_lon) < minmove) {
		// puts("not enough move");
		usleep(SIESTA);
		return;
	}

	/* Don't log if minsecs haven't elapsed since the last fix */
	if ((fabs(int_time - old_int_time) < minsecs) && !first) {
		// puts("too soon");
		usleep(SIESTA);
		return;
	}

	if (first)
		first = FALSE;

	old_int_time = int_time;
	if (minmove > 0) {
		old_lat = fix->latitude;
		old_lon = fix->longitude;
	}

	print_fix(gpsdata, int_time, PERIODIC_REPORT);
}

int main(int argc, char **argv)
{
	unsigned int flags = WATCH_NEWSTYLE; // WATCH_ENABLE | WATCH_JSON;
	long nodata = 0L;
	int keepalive = 60, rc, mid, qos=1;
	char clientid[30], *p, *js;
	struct udata udata;
	JsonNode *jo;

	if ((p = getenv("fixlog")) != NULL) {
		fixlog = fopen(p, "a");
		if (fixlog == NULL) {
			perror(p);
		}
	}

	utarray_new(parms, &ut_str_icd);
	while (*++argv) {
		utarray_push_back(parms, &*argv);
	}




	/*
	 * Obtain topic prefix from environment, and build cmd, state,
	 * and report topic names.
	 */

	if ((p = getenv("tprefix")) != NULL) {
		t_prefix = strdup(p);
	}

	t_report = strdup(t_prefix);
	t_cmd = malloc(strlen(t_prefix) + strlen("/cmd/+") + 1);
	sprintf(t_cmd, "%s/cmd/+", t_prefix);

	t_state = malloc(strlen(t_prefix) + strlen("/state") + 1);
	sprintf(t_state, "%s/state", t_prefix);

	printf("t_report %s\n", t_report);
	printf("t_cmd    %s\n", t_cmd   );
	printf("t_state  %s\n", t_state );

	mosquitto_lib_init();

        sprintf(clientid, "%s-%d", PROGNAME, getpid());
        mosq = mosquitto_new(clientid, TRUE, (void *)&udata);
        if (!mosq) {
                fprintf(stderr, "Out of memory.\n");
                exit(1);
        }

	/* Create payload for LWT consisting of starting timestamp */
	jo = json_mkobject();

	json_append_member(jo, "_type", json_mkstring("lwt"));
	json_append_member(jo, "tst", json_mknumber(time(0)));
	if ((js = json_stringify(jo, NULL)) != NULL) {
		if ((rc = mosquitto_will_set(mosq, t_report, strlen(js), js, QOS1, true)) != MOSQ_ERR_SUCCESS) {
			fprintf(stderr, "Unable to set LWT: %s\n", mosquitto_strerror(rc));
		}
		free(js);
	}
	json_delete(jo);

        mosquitto_publish_callback_set(mosq, cb_pub);
        mosquitto_disconnect_callback_set(mosq, cb_disconnect);
	mosquitto_message_callback_set(mosq, cb_message);

        if ((rc = mosquitto_connect(mosq, MQTT_HOST, MQTT_PORT, keepalive)) != MOSQ_ERR_SUCCESS) {
                fprintf(stderr, "Unable to connect to %s:%d: %s\n", MQTT_HOST, MQTT_PORT,
                        mosquitto_strerror(rc));
                perror("");
                exit(2);
        }

	mosquitto_subscribe(mosq, &mid, t_cmd, qos);

        signal(SIGINT, catcher);

	mosquitto_loop_start(mosq);

	if (gps_open("127.0.0.1", DEFAULT_GPSD_PORT, &gpsdata) != 0) {
		fprintf(stderr,
		      "%s: no gpsd running or network error: %d, %s\n",
		      argv[0], errno, gps_errstr(errno));
		exit(EXIT_FAILURE);
	}

	gps_stream(&gpsdata, flags, NULL);

	while (1) {
		if (!gps_waiting(&gpsdata, 50000000)) {
			printf("no data\n");
			if (++nodata > 10)
				break;
		} else {
			errno = 0;
			if (gps_read(&gpsdata) == -1) {
				perror("gps_read");
				(void)gps_close(&gpsdata);
				exit(1);
			} else {
				if (gpsdata.status == STATUS_NO_FIX) {
					printf("status: NO FIX\n");
				} else {
					conditionally_log_fix(&gpsdata);
				}
			}
		}
	    }

	gps_stream(&gpsdata, WATCH_DISABLE, NULL);
	gps_close(&gpsdata);

	utarray_free(parms);

        mosquitto_disconnect(mosq);
        mosquitto_loop_stop(mosq, FALSE);
        mosquitto_lib_cleanup();

	exit(EXIT_SUCCESS);
}
