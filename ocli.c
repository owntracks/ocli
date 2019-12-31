/*
 * OwnTracks CLI
 * Copyright (C) 2016-2020 Jan-Piet Mens <jpmens@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

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
#include "utstring.h"

#define BLEN		8192

#define QOS0		0
#define QOS1		1
#define QOS2		2
#define SSL_VERIFY_PEER (1)
#define SSL_VERIFY_NONE (0)

#define max(a, b) ( (a > b) ? a : b )

#define PERIODIC_REPORT	NULL
#define ONDEMAND_REPORT "m"

/* Globals, configured from environment */
FILE *fixlog = NULL;		/* if open, record JSON to this file */

UT_array *parms;
static struct gps_data_t gpsdata;
long npubs = 0L;
#if GPSD_API_MAJOR_VERSION >= 7
char gpsmessage[BLEN];
size_t gpsmessagelen = BLEN;
#endif

#define PROGNAME "ocli"
#define SIESTA	700		/* microseconds */

struct udata {
	struct mosquitto *mosq;
	char *clientid;
	char *basetopic;
	char *t_dump;		/* topic on which to dump configuration to */
	char *t_cmd;		/* topic on which to subscribe to for commands */
	char *tid;
	char *username;
	char *device;

	int interval;		/* publish after seconds */
	int displacement;	/* publish after this number of meters movement (def: 0) */
};

void publish(struct udata *ud, char *topic, char *payload, int qos, bool retain);
static void print_fix(struct udata *ud, struct gps_data_t *gpsdata, double time, char *reporttype);

void catcher(int sig)
{
	fprintf(stderr, "%s: Going down on signal %d", PROGNAME, sig);
	exit(1);
}

void publish(struct udata *ud, char *topic, char *payload, int qos, bool retain)
{
	int rc;

	rc = mosquitto_publish(ud->mosq, NULL, topic, strlen(payload), payload, qos, retain);
	if (rc != MOSQ_ERR_SUCCESS) {
		fprintf(stderr, "%s: cannot publish: %s\n", PROGNAME, mosquitto_strerror(rc));
		exit(1);
	}
}

void cb_connect(struct mosquitto *mosq, void *userdata, int reason)
{
	struct udata *ud = (struct udata *)userdata;
	int rc;

	if ((rc = mosquitto_subscribe(ud->mosq, NULL, ud->t_cmd, QOS1)) != MOSQ_ERR_SUCCESS) {
		fprintf(stderr, "cannot subscribe to %s: %s\n", ud->t_cmd,
			mosquitto_strerror(rc));
	}
}

void cb_disconnect(struct mosquitto *mosq, void *userdata, int reason)
{
	if (reason == 0) {
		// Disconnect requested by client
	} else {
		fprintf(stderr, "%s: disconnected: reason: %d (%s)\n",
			PROGNAME, reason, strerror(errno));
		sleep(2);
	}
}

static void config_dump(struct udata *ud)
{
	char *json_string;
	JsonNode *jo;

	jo = json_mkobject();

	json_append_member(jo, "_type",			json_mkstring("configuration"));
	json_append_member(jo, "_npubs",		json_mknumber(npubs));		// nonstandard
	json_append_member(jo, "clientId", 		json_mkstring(ud->clientid));
	json_append_member(jo, "locatorInterval",	json_mknumber(ud->interval));
	json_append_member(jo, "locatorDisplacement",	json_mknumber(ud->displacement));
	json_append_member(jo, "pubTopicBase",		json_mkstring(ud->basetopic));
	if (ud->tid && *ud->tid) {
		json_append_member(jo, "tid", 			json_mkstring(ud->tid));
	}
	json_append_member(jo, "username", 		json_mkstring(ud->username));
	json_append_member(jo, "deviceId", 		json_mkstring(ud->device));

	if ((json_string = json_stringify(jo, NULL)) != NULL) {
		publish(ud, ud->t_dump, json_string, QOS1, false);

		free(json_string);
	}

	json_delete(jo);
}

void set_config(struct udata *ud, JsonNode *json, char *payload)
{
	JsonNode *conf, *j;

	/* FIXME: add checking for remoteConfiguration to permit changes */

	if ((conf = json_find_member(json, "configuration")) == NULL) {
		fprintf(stderr, "No configuration in JSON %s\n", (char *)payload);
		return;
	}

	/*
	 * We now have:
	 * {
	 *     "_type": "configuration",
	 *     "bla": "foo"
	 * }
	 */

	if (((j = json_find_member(conf, "_type")) == NULL) ||
		(strcmp(j->string_, "configuration") != 0)) {
		fprintf(stderr, "No configuration in config action %s\n", payload);
		return;
	}

	json_foreach(j, conf) {
		if (strcmp(j->key, "_type") == 0)
			continue;
		// printf("%s\t%s\n", j->key, j->string_);

		if (strcmp(j->key, "locatorInterval") == 0) {
			ud->interval = j->number_;
			fprintf(stderr, "Set interval to %d\n", ud->interval);
		} else if (strcmp(j->key, "locatorDisplacement") == 0) {
			ud->displacement = j->number_;
			fprintf(stderr, "Set displacement to %d\n", ud->displacement);
		} else {
			fprintf(stderr, "Ignoring unknown configuration key `%s'\n", j->key);
		}
	}
}

void cb_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg)
{
	struct udata *ud = (struct udata *)userdata;
	JsonNode *json, *j;
	char *action = NULL;

	if (msg->payloadlen < 2)
		return;

	if ((json = json_decode((char *)msg->payload)) == NULL) {
		fprintf(stderr, "Cannot decode JSON from %s\n", (char *)msg->payload);
		return;
	}

	if (((j = json_find_member(json, "_type")) == NULL) ||
		(strcmp(j->string_, "cmd") != 0)) {
		fprintf(stderr, "No cmd in JSON %s\n", (char *)msg->payload);
		json_delete(json);
		return;
	}

	if ((j = json_find_member(json, "action")) == NULL) {
		fprintf(stderr, "No action in JSON %s\n", (char *)msg->payload);
		json_delete(json);
		return;
	}

	if (j->tag != JSON_STRING) {
		fprintf(stderr, "action tag is not string in JSON %s\n", (char *)msg->payload);
		json_delete(json);
		return;
	}

	action = strdup(j->string_);

	if (strcmp(action, "dump") == 0) {
		config_dump(ud);
	} else if (strcmp(action, "reportLocation") == 0) {
		print_fix(ud, &gpsdata, (time_t)time(0), ONDEMAND_REPORT);
	} else if (strcmp(action, "setConfiguration") == 0) {
		set_config(ud, json, (char *)msg->payload);
	}

	json_delete(json);
	if (action)
		free(action);
}

static void print_fix(struct udata *ud, struct gps_data_t *gpsdata, double ttime, char *reporttype)
{
	char buf[128], *json_string, **p;
	double accuracy;
	struct gps_fix_t *fix = &(gpsdata->fix);
	JsonNode *jo;
	char tbuf[128];

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

	unix_to_iso8601(ttime, tbuf, sizeof(tbuf));
	printf("mode=%d, lat=%f, lon=%f, acc=%f, tst=%s (%ld)\n",
		fix->mode,
		fix->latitude,
		fix->longitude,
		accuracy,
		tbuf,
		(long)ttime);

	jo = json_mkobject();

	json_append_member(jo, "_type",	json_mkstring("location"));
	json_append_member(jo, "lat",	json_mknumber(fix->latitude));
	json_append_member(jo, "lon",	json_mknumber(fix->longitude));
	json_append_member(jo, "acc",	json_mknumber((long)accuracy));
	json_append_member(jo, "tst",	json_mknumber(ttime));

	if (ud->tid && *ud->tid) {
		json_append_member(jo, "tid", json_mkstring(ud->tid));
	}

	if (reporttype) {
		json_append_member(jo, "t", json_mkstring(reporttype));
	}

	/* DEBUGGING */
	sprintf(buf, "%d/%d", gpsdata->satellites_used, gpsdata->satellites_visible);
	json_append_member(jo, "sat", json_mkstring(buf));
	if ((fix->mode == MODE_3D) && !isnan(fix->altitude)) {
		json_append_member(jo, "alt", json_mknumber((long)fix->altitude));
	}
	if (!isnan(fix->speed)) {
		long kph = (fix->speed * 3.6); 	/* meters/s -> km/h */
		json_append_member(jo, "vel", json_mknumber(kph));
	}

	/*
	 * For each of the filenames passed as parameters, use the basename
	 * as key for the string value of the first line in the file and add
	 * to the JSON. Skip if the key is already in the JSON object, i.e.
	 * don't overwrite existing elements.
	 */

	p = NULL;
	while ((p = (char**)utarray_next(parms, p))) {
		char *key = basename(*p), *val = NULL;
		FILE *fp;
		bool is_exec = false;

		if (json_find_member(jo, key) != NULL) {
			// fprintf(stderr, "Refuse to overwrite key=%s\n", key);
			continue;
		}

		is_exec = access(*p, X_OK) == 0;

		if (is_exec) {
			fp = popen(*p, "r");
			is_exec = true;
		} else {
			fp = fopen(*p, "r");
		}

		if (fp != NULL) {
			char buf[1025], *bp;
			if (fgets(buf, sizeof(buf), fp) != NULL) {
				if ((bp = strchr(buf, '\r')) != NULL)
					*bp = 0;
				if ((bp = strchr(buf, '\n')) != NULL)
					*bp = 0;
				val = buf;
			}
			if (is_exec) {
				pclose(fp);
			} else {
				fclose(fp);
			}
		}
		if (val) {
			json_append_member(jo, key, json_mkstring(val));
		} else {
			json_append_member(jo, key, json_mknull());
		}
	}

	if ((json_string = json_stringify(jo, NULL)) != NULL) {
		npubs++;
		publish(ud, ud->basetopic, json_string, QOS1, true);

		if (fixlog) {
			fprintf(fixlog, "%s\n", json_string);
			fflush(fixlog);	/* FIXME */
		}
		free(json_string);
	}

	json_delete(jo);
}


static void conditionally_log_fix(struct udata *ud, struct gps_data_t *gpsdata)
{
	struct gps_fix_t *fix = &(gpsdata->fix);
	static double int_time, old_int_time;
	static double old_lat, old_lon;
	static bool first = true;
	bool valid = false;

	if (gpsdata->set & POLICY_SET) {
		gpsdata->set &= ~(POLICY_SET);
		return;
	}

	if (gpsdata->set & STATUS_SET) {
		switch (gpsdata->status) {
			case STATUS_FIX:
#ifdef STATUS_DGPS_FIX
			case STATUS_DGPS_FIX:
#endif
				switch (gpsdata->fix.mode) {
					case MODE_2D:
						if (gpsdata->set & LATLON_SET) {
							valid = true;
						}
						break;

					case MODE_3D:
						if (gpsdata->set & (LATLON_SET|ALTITUDE_SET)) {
							valid = true;
						}
						break;

					case MODE_NOT_SEEN:
						fprintf(stderr, ".. fix not yet seen\n");
						break;

					case MODE_NO_FIX:
						fprintf(stderr, ".. no fix yet\n");
						break;

					default:
						fprintf(stderr, ".. unpossible mode\n");
						break;
				}
				break;

			case STATUS_NO_FIX:
				fprintf(stderr, ".. no fix\n");
				break;

			default:
				fprintf(stderr, "status == %llu\n", gpsdata->set & MODE_SET);
				break;
		}
	}

#if 0
#if GPSD_API_MAJOR_VERSION >= 7
	char *bp;
	if ((bp = strchr(gpsmessage, '\r')) != NULL)
		*bp = 0;
	fprintf(stderr, "(%zu) %s\n", gpsmessagelen, gpsmessage);
#endif
#endif

	if (valid == false)
		return;

	int_time = fix->time;

	if ((int_time == old_int_time) || fix->mode < MODE_2D) {
		// puts("rubbish");
		usleep(SIESTA);
		return;
	}

	/* may not be worth logging if we've moved only a very short distance */
	if (ud->displacement>0 && !first && earth_distance(
					fix->latitude,
					fix->longitude,
					old_lat, old_lon) < ud->displacement) {
		// puts("not enough move");
		usleep(SIESTA);
		return;
	}

	/* Don't log if interval seconds haven't elapsed since the last fix */
	if ((fabs(int_time - old_int_time) < ud->interval) && !first) {
		// puts("too soon");
		usleep(SIESTA);
		return;
	}

	if (first)
		first = false;

	old_int_time = int_time;
	if (ud->displacement > 0) {
		old_lat = fix->latitude;
		old_lon = fix->longitude;
	}

	print_fix(ud, gpsdata, int_time, PERIODIC_REPORT);
}

static int env_number(char *key, int min)
{
	char *p;
	int n;

	if ((p = getenv(key)) == NULL)
		return (min);
	if ((n = atoi(p)) < min)
		n = min;
	return (n);
}

int main(int argc, char **argv)
{
	unsigned int flags = WATCH_NEWSTYLE; // WATCH_ENABLE | WATCH_JSON;
	int keepalive = 60, rc;
	char *p, *js;
	char *gpsd_host = "localhost", *gpsd_port = DEFAULT_GPSD_PORT;
	char *mqtt_host = "localhost";
	short mqtt_port = 1883;
	struct udata udata, *ud = &udata;
	char hostname[BUFSIZ], *h, *username;
	JsonNode *jo;
	char *cacert = NULL;

	ud->clientid = NULL;
	ud->interval = env_number("OCLI_INTERVAL", 60);		// minsecs seconds
	ud->displacement = env_number("OCLI_DISPLACEMENT", 0);	// minmove meters

	if ((p = getenv("GPSD_HOST")) != NULL)
		gpsd_host = strdup(p);

	if ((p = getenv("GPSD_PORT")) != NULL) {
		gpsd_port = atoi(p) < 1 ? DEFAULT_GPSD_PORT : p;
	}

	if ((p = getenv("MQTT_HOST")) != NULL)
		mqtt_host = strdup(p);

	if ((p = getenv("MQTT_PORT")) != NULL) {
		mqtt_port = atoi(p) < 1 ? 1883 : atoi(p);
	}

	if ((p = getenv("OCLI_CACERT")) != NULL) {
		cacert = strdup(p);
	}

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

	if ((username = getlogin()) == NULL)
		username = "nobody";

	if (gethostname(hostname, sizeof(hostname)) != 0)
		strcpy(hostname, "localhost");

	if ((h = strchr(hostname, '.')) != NULL)
		*h = 0;

	/*
	 * Build MQTT topic name which defaults to owntracks/user/device
	 * but can be overriden from the environment.
	 */

	if ((p = getenv("BASE_TOPIC")) != NULL) {
		ud->basetopic = strdup(p);
	} else {
		UT_string *to;

		utstring_new(to);
		utstring_printf(to, "owntracks/%s/%s", username, hostname);
		ud->basetopic = strdup(utstring_body(to));

		ud->username = strdup(username);
		ud->device =  strdup(hostname);

	}
	ud->tid = getenv("OCLI_TID");		/* may be null */

	if ((p = getenv("OCLI_CLIENTID")) != NULL) {
		ud->clientid = strdup(p);
	} else {
		UT_string *cid;

		utstring_new(cid);
		utstring_printf(cid, "ocli-%s-%s", username, hostname);
		ud->clientid = strdup(utstring_body(cid));
	}

	ud->t_cmd = malloc(strlen(ud->basetopic) + strlen("/cmd") + 1);
	sprintf(ud->t_cmd, "%s/cmd", ud->basetopic);

	ud->t_dump = malloc(strlen(ud->basetopic) + strlen("/dump") + 1);
	sprintf(ud->t_dump, "%s/dump", ud->basetopic);

	printf("t_report %s\n", ud->basetopic);
	printf("t_cmd    %s\n", ud->t_cmd);
	printf("t_dump   %s\n", ud->t_dump );

	mosquitto_lib_init();

	ud->mosq = mosquitto_new(ud->clientid, true, (void *)&udata);
	if (!ud->mosq) {
		fprintf(stderr, "Out of memory.\n");
		exit(1);
	}

	if (cacert != NULL) {
		rc = mosquitto_tls_set(ud->mosq,
			cacert,      /* cafile */
			NULL,        /* capath */
			NULL,        /* certfile */
			NULL,        /* keyfile */
			NULL         /* pw_callback() */
			);
		if (rc != MOSQ_ERR_SUCCESS) {
			fprintf(stderr, "%s: cannot set TLS CA: %s (check path names)\n",
				PROGNAME, mosquitto_strerror(rc));
			exit(3);
		}

		mosquitto_tls_opts_set(ud->mosq,
			SSL_VERIFY_PEER,
			NULL,                   /* tls_version: "tlsv1.2", "tlsv1" */
			NULL                    /* ciphers */
			);
	}



	/* Create payload for LWT consisting of starting timestamp */
	jo = json_mkobject();

	json_append_member(jo, "_type", json_mkstring("lwt"));
	json_append_member(jo, "tst", json_mknumber(time(0)));
	if ((js = json_stringify(jo, NULL)) != NULL) {
		if ((rc = mosquitto_will_set(ud->mosq, ud->basetopic, strlen(js), js, QOS1, true)) != MOSQ_ERR_SUCCESS) {
			fprintf(stderr, "Unable to set LWT: %s\n", mosquitto_strerror(rc));
		}
		free(js);
	}
	json_delete(jo);

	mosquitto_connect_callback_set(ud->mosq, cb_connect);
	mosquitto_disconnect_callback_set(ud->mosq, cb_disconnect);
	mosquitto_message_callback_set(ud->mosq, cb_message);

	if ((rc = mosquitto_connect(ud->mosq, mqtt_host, mqtt_port, keepalive)) != MOSQ_ERR_SUCCESS) {
		fprintf(stderr, "Unable to connect to %s:%d: %s\n", mqtt_host, mqtt_port,
			mosquitto_strerror(rc));
		perror("");
		exit(2);
	}

	signal(SIGINT, catcher);
	signal(SIGTERM, catcher);

	mosquitto_loop_start(ud->mosq);

	if (gps_open(gpsd_host, gpsd_port, &gpsdata) != 0) {
		fprintf(stderr,
		      "%s: no gpsd running or network error: %d, %s\n",
		      argv[0], errno, gps_errstr(errno));
		exit(EXIT_FAILURE);
	}

	gps_stream(&gpsdata, flags, NULL);

	while (1) {
		if (gps_waiting(&gpsdata, 5000000) == true) {
			errno = 0;
#if GPSD_API_MAJOR_VERSION >= 7
			if (gps_read(&gpsdata, gpsmessage, gpsmessagelen) == -1) {
#else
			if (gps_read(&gpsdata) == -1) {
#endif
				if (errno == 0) {
					// lost contact with gpsd
					break;
				}
			} else {
				conditionally_log_fix(&udata, &gpsdata);
			}
		}
	}

	gps_stream(&gpsdata, WATCH_DISABLE, NULL);
	gps_close(&gpsdata);

	utarray_free(parms);

	mosquitto_disconnect(ud->mosq);
	mosquitto_loop_stop(ud->mosq, false);
	mosquitto_lib_cleanup();

	exit(EXIT_SUCCESS);
}
