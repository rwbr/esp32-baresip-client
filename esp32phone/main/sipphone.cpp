
#define TAG "sipphone"
#include "esp_log.h"
#include "sipphone.h"
#include "SPIFFS.h"

#ifdef ENABLE_baresip

typedef uint32_t u32_t;
#include <net/if.h>
#include <re.h>
#include <baresip.h>
#include <string.h>

static TaskHandle_t baresip_thread;

static void baresipLogFunc(uint32_t level, const char *msg)
{
	(void)level;

    ESP_LOGI(TAG, "%s", msg);
}

static struct log baresipLog = {
	LE_INIT,
	baresipLogFunc
};


static void ua_exit_handler(void *arg)
{
	(void)arg;
	ESP_LOGW(TAG, "ua exited -- stopping main runloop\n");

	/* The main run-loop can be stopped now */
	re_cancel();
}

static void ua_event_handler(struct ua *ua, enum ua_event ev,
			     struct call *call, const char *prm, void *arg)
{
	re_printf("ua event: %s\n", uag_event_str(ev));
}


static void signal_handler(int sig)
{
	static bool term = false;

	if (term) {
		mod_close();
		exit(0);
	}

	term = true;

	ESP_LOGW(TAG, "terminated by signal %d\n", sig);

	ua_stop_all(false);
}

void baresip_main(void* arg)
{
	int err = 0;

	ESP_LOGI(TAG, "Entered baresip thread");

	err = re_main(signal_handler);

	if (err)
		ua_stop_all(true);

	ua_close();
	conf_close();

	baresip_close();

	/* NOTE: modules must be unloaded after all application
	 *       activity has stopped.
	 */
	ESP_LOGW(TAG, "main: unloading modules..\n");
	mod_close();

	log_unregister_handler(&baresipLog);

	libre_close();

	/* Check for memory leaks */
	tmr_debug();
	mem_debug();

	ESP_LOGI(TAG, "Exiting baresip thread");

	return;
}

int sipPhoneInit() {
  const char *sipTransportLayerStr = "udp";
	const char *bfsipVersionStr = "0.1";
	char versionBuffer[256];
	bool udp = true, tcp = true, tls = true, prefer_ipv6 = false;
	int err = 0;
	FILE *pTempFile = NULL;

	ESP_LOGI(TAG, "Starting Baresip");

	err = libre_init();
	if (err) {
		ESP_LOGE(TAG, "Could not initialize libre");
		goto baresip_error;
	}


    ESP_LOGI(TAG, "Baresip logging activated");
    log_register_handler(&baresipLog);
    log_enable_debug(true);

	conf_path_set("/var/tmp/baresip");
	ESP_LOGI(TAG, "Baresip %s: %d", __FUNCTION__, __LINE__);

	err = conf_configure();
	if (err) {
		ESP_LOGE(TAG, "Could not configure baresip");
		goto baresip_error;
	}
	ESP_LOGI(TAG, "Baresip %s: %d", __FUNCTION__, __LINE__);

	err = baresip_init(conf_config(), false);
	ESP_LOGI(TAG, "Baresip %s: %d", __FUNCTION__, __LINE__);

	if (err) {
		ESP_LOGE(TAG, "Could not initialize baresip");
		goto baresip_error;
	}
	ESP_LOGI(TAG, "Baresip %s: %d", __FUNCTION__, __LINE__);

	 if (str_isset(conf_config()->audio.audio_path)) {
		play_set_path(baresip_player(),
				  conf_config()->audio.audio_path);
	}

	if (sipTransportLayerStr && !strcmp(sipTransportLayerStr, "TLS_ONLY")) {
		udp = false;
		tcp = false;
		ESP_LOGI(TAG, "Baresip listening to TLS only");
	}

	*versionBuffer = 0;
	strncat(versionBuffer, "commend SIP Series ", sizeof(versionBuffer));
	if (bfsipVersionStr) {
		char *pFound;

		strncat(versionBuffer, bfsipVersionStr, sizeof(versionBuffer));

		pFound = strchr(versionBuffer, '(');
		if (pFound)
			*(pFound - 1) = 0; // set space in front of '(' to zero
	} else {
		ESP_LOGW(TAG, "BF-SIP version undefined");
		strncat(versionBuffer, "X", sizeof(versionBuffer));
	}

	ESP_LOGI(TAG, "Using this as baresip version string: %s", versionBuffer);

	err = ua_init(versionBuffer, udp, tcp, tls, prefer_ipv6);
	if (err) {
		ESP_LOGE(TAG, "Could not initialize baresip user agent");
		goto baresip_error;
	}

	uag_set_exit_handler(ua_exit_handler, NULL);
	uag_event_register(ua_event_handler, NULL);

	err = conf_modules();
	if (err) {
		ESP_LOGE(TAG, "Could not configure baresip modules");
		goto baresip_error;
	}

	xTaskCreate(baresip_main, "baresipmain", 2048, NULL, 10, &baresip_thread);  

	ESP_LOGI(TAG, "Baresip initialization done");

	return true;

baresip_error:
  ua_stop_all(true);

  ua_close();
  conf_close();

  baresip_close();

  /* NOTE: modules must be unloaded after all application
    *       activity has stopped.
    */
  ESP_LOGW(TAG, "main: unloading modules..\n");
  mod_close();

  log_unregister_handler(&baresipLog);

  libre_close();

  /* Check for memory leaks */
  tmr_debug();
  mem_debug();

  return false;
}

#ifdef __cplusplus
extern "C" {
#endif

	FILE *spiff_open (const char * filename, const char * modes) {
		ESP_LOGI(TAG, "%s: %d", __FUNCTION__, __LINE__);
		char* mode = FILE_READ;

		if (String(mode).indexOf("a")>=0)
			mode = FILE_APPEND;
		else if (String(mode).indexOf("w")>=0)
			mode = FILE_WRITE;

		File* file = new File();
		*file = SPIFFS.open(filename, mode);
		if (!file) {
			Serial.println("ERROR opening file ... with mode ...");
			ESP_LOGE(TAG, "ERROR opening file %s with mode %s", filename, mode);
			return 0;
		}
		ESP_LOGI(TAG, "%s: %d", __FUNCTION__, __LINE__);
		return (FILE*) file;
	}

	size_t spiff_printf(FILE* f, char* message, size_t len) {
		ESP_LOGI(TAG, "spiff_print: ...");
		/*char* tmp = (char*) malloc(len+1);
		snprintf(tmp, len, "%s", message);
		ESP_LOGI(TAG, "spiff_print: %s done", tmp);
		free(tmp);
		return len;*/
		return ((File*) f)->write((uint8_t*) message, len);
	}

	size_t spiff_read(FILE* f, void * buf, size_t len) {
		return ((File*) f)->read(((uint8_t*)buf), len);
	}

	void spiff_close(FILE* f) {
		//return;
		((File*) f)->close();
	}


// TODO: use correct function instead of dummy impl
int net_rt_list(net_rt_h *rth, void *arg) {
	return 0;
}


#include <signal.h> 
#include <pwd.h>

sighandler_t signal(int signum, sighandler_t handler)
{
	return (sighandler_t) 0;
}

char *  getlogin(void) {
	return (char*) "";
}


struct passwd	*getpwnam (const char *) {
	return 0;
}

// libre apis to load modules (which we use statically)
void *_mod_open(const char *name) {
	ESP_LOGI(TAG, "%s: %d", __FUNCTION__, __LINE__);
	return 0;
}

void *_mod_sym(void *h, const char *symbol) {
	ESP_LOGI(TAG, "%s: %d", __FUNCTION__, __LINE__);
	return 0;
}
void  _mod_close(void *h) {
	ESP_LOGI(TAG, "%s: %d", __FUNCTION__, __LINE__);
}



const struct mod_export *mod_table[] = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	// TODO provide functions in table
	/*&exports_wincons,
	&exports_g711,
	&exports_winwave,
	&exports_dshow,
	&exports_account,
	&exports_contact,
	&exports_menu,
	&exports_auloop,
	&exports_vidloop,
	&exports_uuid,
	&exports_stun,
	&exports_turn,
	&exports_ice,
	&exports_vumeter,*/
	NULL
};


#ifdef __cplusplus
}
#endif


#else //ENABLE_baresip
int sipPhoneInit() {
	return true;
}
#endif