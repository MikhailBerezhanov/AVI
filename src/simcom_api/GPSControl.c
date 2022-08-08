#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <pthread.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include <sys/mman.h>

#include <xtra_system_interface.h>
#include <location_interface.h>
#include "GPSControl.h"

gps_ind_cb_fcn test_gps_ind_cb;

static boolean is_xtra_use = FALSE;

// Required by Xtra Client init function
XtraClientDataCallbacksType myXtraClientDataCallback = {
    test_xtra_client_data_callback
};

// Required by Xtra Client function
XtraClientTimeCallbacksType myXtraClientTimeCallback = {
    test_xtra_client_time_callback
};

static LocationInterface * location_test_interface_ptr = NULL;
static XtraClientInterfaceType *pXtraClientInterface = NULL;

// Xtra data file
FILE * g_xtra_data_file;

XtraClientConfigType g_xtra_client_config = {
    XTRA_NTP_DEFAULT_SERVER1,
    XTRA_DEFAULT_SERVER1,
    XTRA_DEFAULT_SERVER2,
    XTRA_DEFAULT_SERVER3,
    XTRA_USER_AGENT_STRING
};

static garden_ext_cb_t my_garden_cb = {
    &test_garden_location_cb,
    &test_garden_status_cb,
    &test_garden_sv_status_cb,
    &test_garden_nmea_cb,
    &test_garden_set_capabilities_cb,
    &test_garden_xtra_time_req_cb,
    &test_garden_xtra_report_server_cb,
    &test_garden_xtra_download_req_cb,
    &test_agps_status_cb,
    &test_garden_ni_notify_cb,
    &test_garden_acquire_wakelock_cb,
    &test_garden_release_wakelock_cb,
    &test_garden_create_thread_cb
};


void garden_print(const char *fmt, ...)
{
    va_list ap;
    char buf[1024];
    va_start(ap, fmt);
    vsnprintf(buf, 1024, fmt, ap);
    va_end(ap);
    fprintf(stderr,"GARDEN: %s\n",buf);
}

typedef void (*ThreadStart) (void *);
struct tcreatorData {
    ThreadStart pfnThreadStart;
    void* arg;
};

void *my_thread_fn (void *tcd)
{
    struct tcreatorData* local_tcd = (struct tcreatorData*)tcd;
    if (NULL != local_tcd) {
        local_tcd->pfnThreadStart (local_tcd->arg);
        free(local_tcd);
    }
    return NULL;
}

// location-xtra library calls this function back when data is ready
void test_xtra_client_data_callback(char * data,int length) {

    g_xtra_data_file = fopen(XTRA_DATA_FILE_NAME,"w");

    if(g_xtra_data_file != NULL)
    {
        fwrite(data,1,length,g_xtra_data_file);
        // close file
        fclose(g_xtra_data_file);
    }
	garden_print("## test_xtra_client_data_callback data_length=%d ##", length);
	location_test_interface_ptr->location_xtra_inject_data(data, length);

}

// location-xtra library calls this function back when time is ready
void test_xtra_client_time_callback(int64_t utcTime, int64_t timeReference, int uncertainty) {
	garden_print("## test_xtra_client_time_callback ##");
	location_test_interface_ptr->location_inject_time(utcTime, timeReference, uncertainty);
}

/*timestamp tranfer*/
void transformToDate(int64_t timeStamp ,uint8 *dateArry)
{  
    int64_t low ,high ,mid ,t;  
    int64_t year ,month ,day ,hour ,minute ,second ,milliSecond;  
    int64_t daySum[] = {0 ,31 ,59 ,90 ,120 ,151 ,181 ,212 ,243 ,273 ,304 ,334 ,365};  
    int64_t milOfDay = 24 * 3600 * 1000;  
    int64_t milOfHour = 3600 * 1000;  
   
    if(timeStamp > 315537897599999) {  
        timeStamp = 315537897599999;  
    }  
   
    low = 1;  
    high = 9999;  
   
    while(low <= high)  
    {  
        mid = (low+high)>>1;  
        t = ((mid-1) * 365 + (mid-1)/4 - (mid-1)/100 + (mid-1)/400) * milOfDay;  
   
        if(t == timeStamp)  
        {  
            low = mid + 1;  
            break;  
        }  
        else if(t < timeStamp)  
            low = mid + 1;  
        else  
            high = mid - 1;  
    }  
    year = low-1;  
    timeStamp -= ((year-1) * 365 + (year-1)/4 - (year-1)/100 + (year-1)/400) * milOfDay;  
   
    int isLeapYear = ((year&3) == 0 && year%100!=0) || year%400 == 0;  
   
    for(month = 1 ;(daySum[month] + ((isLeapYear && month > 1) ? 1 : 0)) * milOfDay <= timeStamp && month < 13 ;month ++) {  
        if(isLeapYear && month > 1)  
            ++daySum[month];  
    }  
    timeStamp -= daySum[month-1] * milOfDay;  
   
    day = timeStamp / milOfDay;  
    timeStamp -= day * milOfDay;  
   
    hour = timeStamp / milOfHour;  
    timeStamp -= hour * milOfHour;  
   
    minute = timeStamp / 60000;  
    timeStamp -= minute * 60000;  
   
    second = timeStamp / 1000;  
    milliSecond = timeStamp % 1000;  
   
    dateArry[0] = (uint8)(year+1970-1-2000);  
    dateArry[1] = (uint8)month;  
    dateArry[2] = (uint8)day+1;  
    dateArry[3] = (uint8)hour;  
    dateArry[4] = (uint8)minute;  
    dateArry[5] = (uint8)second;  
}

void test_garden_location_cb (GpsLocation * location) {
	GpsInfo gps_info;

	transformToDate(location->timestamp, gps_info.time);
	gps_info.accuracy = location->accuracy;
	gps_info.bearing = location->bearing;
	gps_info.speed = location->speed;
	gps_info.altitude = location->altitude;
	gps_info.longitude = location->longitude;
	gps_info.latitude = location->latitude;
	test_gps_ind_cb(SIMCOM_EVENT_LOC_IND, (void*)&gps_info);
    //garden_print("\nLATITUDE:  %f\nLONGITUDE: %f\nALTITUDE:  %f\nSPEED:     %f\nTIME:      %s",
    //             location->latitude, location->longitude, location->altitude,location->speed, time);
}


void test_garden_status_cb (GpsStatus * status) {
    if(status->status == GPS_STATUS_ENGINE_ON)
    {
    	garden_print("## gps engine on ##");
    }

    if(status->status == GPS_STATUS_ENGINE_OFF)
    {
    	garden_print("## gps engine off  ##");
    }
}

void test_garden_sv_status_cb (GpsSvStatus * sv_info)
{

}

void test_garden_nmea_cb (GpsUtcTime timestamp, const char *nmea, int length)
{
	char tmp[300] = {0};
    if(nmea != NULL) {
		memcpy(tmp, nmea, length);
		test_gps_ind_cb(SIMCOM_EVENT_NMEA_IND, (void *)tmp);
		//garden_print(tmp);
	}
}

void test_garden_set_capabilities_cb (uint32_t capabilities) {
}

void test_garden_acquire_wakelock_cb () {
}

void test_garden_release_wakelock_cb () {
}

pthread_t test_garden_create_thread_cb (const char *name, void (*start) (void *),
                                void *arg)
{
    garden_print("## gps_create_thread ##:");
    pthread_t thread_id = -1;
    garden_print ("%s", name);

    struct tcreatorData* tcd = (struct tcreatorData*)malloc(sizeof(*tcd));

    if (NULL != tcd) {
        tcd->pfnThreadStart = start;
        tcd->arg = arg;

        if (0 > pthread_create (&thread_id, NULL, my_thread_fn, (void*)tcd)) {
            garden_print ("error creating thread");
            free(tcd);
        } else {
            garden_print ("created thread");
        }
    }
    return thread_id;
}

void test_garden_xtra_download_req_cb () {
    if(is_xtra_use == FALSE) {
        return;
    }
    garden_print ("## gps_xtra_download_request ##:");
    // Pass on the data request to Xtra Client
    if (pXtraClientInterface != NULL) {
	    pXtraClientInterface->onDataRequest();
    }
}

void test_garden_xtra_time_req_cb () {

    garden_print ("## gps_request_utc_time ##:");
    // Pass on the time request to Xtra Client
    if (pXtraClientInterface != NULL) {
	    pXtraClientInterface->onTimeRequest();
    }
}

void test_garden_xtra_report_server_cb(const char* server1, const char* server2, const char* server3) {
    garden_print ("## report_xtra_server_callback ##:");
    //XtraClientInterfaceType const * pXtraClientInterface = get_xtra_client_interface();
    if (pXtraClientInterface != NULL)
    {
        pXtraClientInterface->onXTRAServerReported(g_xtra_client_config.xtra_server_url[0], g_xtra_client_config.xtra_server_url[1], g_xtra_client_config.xtra_server_url[2]);
    }
}

void test_agps_status_cb (AGpsExtStatus * status) {
}

void test_garden_ni_notify_cb(GpsNiNotification *notification) {
}

static boolean location_init()
{
	int rc = 0;

    location_test_interface_ptr = (LocationInterface*)get_location_client_interface();
    if (NULL == location_test_interface_ptr)
    {
        garden_print("get_location_client_interface null");
        return FALSE;
    }

    rc = location_test_interface_ptr->location_init(&my_garden_cb);

    if(rc == -1) {
        garden_print(" Could not Initialize, Cannot proceed ");
        return FALSE;
    }

    pXtraClientInterface = (XtraClientInterfaceType*)get_xtra_client_interface();
    if (NULL == pXtraClientInterface)
    {
        garden_print("get_xtra_client_interface null");
        return FALSE;
    }

    pXtraClientInterface->init(&myXtraClientDataCallback, &myXtraClientTimeCallback, &g_xtra_client_config);
	return TRUE;

}

static boolean location_close()
{
	if (location_test_interface_ptr == NULL) {
		garden_print("Location Already Close");
		return FALSE;
	}
	location_test_interface_ptr->location_close();
    // Stop XTRA client
    //XtraClientInterfaceType const* pXtraClientInterface = get_xtra_client_interface();
    if(pXtraClientInterface != NULL) {
        pXtraClientInterface->stop();
    }

    location_test_interface_ptr = NULL;
    pXtraClientInterface = NULL;
	return TRUE;
}

static boolean location_xtra_enable()
{
    int s_fd = -1;
    int count;
    char command[50];
    char readbuf[50];
	 int flag = 1;
    
	s_fd = open (S_DEVICE_PATH, O_RDWR);
	if(s_fd < 0)
	{
		printf("[%s] open device fail.\n", __func__);
		return -1;
	}

    /*enable xtra*/
	memset(command, 0, sizeof(command));
	sprintf(command, "AT+CGPSXE=%d\r\n", flag);
	count = write (s_fd, command, strlen(command));
	if(count != strlen(command))
	{
		printf("[%s] write fail.\n", __func__);
		return -1;
	}
    usleep(200*1000);
    memset(readbuf, 0, sizeof(readbuf));
    count = read(s_fd, readbuf,50);
    if (strstr(readbuf, "OK") == NULL) 
    {
        printf("AT+CGPSXE=%d set failure", flag);
        return -1;
    }
	close(s_fd);
	is_xtra_use = TRUE;
	return 0;	
}

static boolean location_xtra_disable()
{
    int s_fd = -1;
    int count;
    char command[50];
    char readbuf[50];
	 int flag = 0;
    
	s_fd = open (S_DEVICE_PATH, O_RDWR);
	if(s_fd < 0)
	{
		printf("[%s] open device fail.\n", __func__);
		return -1;
	}

    /*enable xtra*/
	memset(command, 0, sizeof(command));
	sprintf(command, "AT+CGPSXE=%d\r\n", flag);
	count = write (s_fd, command, strlen(command));
	if(count != strlen(command))
	{
		printf("[%s] write fail.\n", __func__);
		return -1;
	}
    usleep(200*1000);
    memset(readbuf, 0, sizeof(readbuf));
    count = read(s_fd, readbuf,50);
    if (strstr(readbuf, "OK") == NULL) 
    {
        printf("AT+CGPSXE=%d set failure", flag);
        return -1;
    }
	close(s_fd);
	is_xtra_use = FALSE;
	return 0;	
}

static boolean location_coldstart()
{
	if (location_test_interface_ptr == NULL) {
		garden_print("Please Init Location");
		return FALSE;
	}
	location_test_interface_ptr->location_set_position_mode(0, 0, 1000, 10000, 0);
	location_test_interface_ptr->location_delete_aiding_data(0xFFFFFFFF);
	location_test_interface_ptr->location_start_navigation();
	return TRUE;
}

static boolean location_hotstart()
{
	if (location_test_interface_ptr == NULL) {
		garden_print("Please Init Location");
		return FALSE;
	}
	location_test_interface_ptr->location_set_position_mode(0, 0, 1000, 10000, 0);
	location_test_interface_ptr->location_start_navigation();
	return TRUE;
}

static boolean location_stop()
{
	if (location_test_interface_ptr == NULL) {
		garden_print("Please Init Location");
		return FALSE;
	}
	location_test_interface_ptr->location_stop_navigation();
	return TRUE;
}

/*===============================================================================*/

boolean gps_init(gps_ind_cb_fcn gps_ind_cb)
{
	test_gps_ind_cb = gps_ind_cb;
	return location_init();
}

void gps_deinit()
{
	location_close();
}
boolean gps_xtra_enable()
{
	return location_xtra_enable();
}
boolean gps_xtra_disable()
{
	return location_xtra_disable();
}
boolean gps_coldstart()
{
	return location_coldstart();
}
boolean gps_hotstart()
{
	return location_hotstart();
}
boolean gps_stop()
{
	return location_stop();
}

