/*==============================================================================
MIT License

Copyright (c) 2023 Trevor Monk

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
==============================================================================*/

/*!
 * @defgroup neurio neurio
 * @brief Neurio CT Sensor Interface
 * @{
 */

/*============================================================================*/
/*!
@file neurio.c

    Neurio

    The Neurio VT Sensor Interface Application interogates a
    Neurio CT sensor and stores the retrieved data to system
    variables on a pre-defined interval.

*/
/*============================================================================*/

/*==============================================================================
        Includes
==============================================================================*/

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <varserver/varserver.h>
#include <tjson/json.h>
#include <curl/curl.h>

/*==============================================================================
        Private definitions
==============================================================================*/

/*! default broker address */
#define ADDRESS     "192.168.86.31"

/*! Memory Buffer for curl responses */
typedef struct _rxBuffer
{
    /*! pointer to the rx buffer */
    char *p;

    /*! size of the rx buffer */
    size_t size;

    /*! number of bytes in the rx buffer which are used */
    size_t len;

    /*! bytes remaining in the buffer */
    size_t remaining;

} RxBuffer;

/*! Neurio state */
typedef struct neurioState
{
    /*! variable server handle */
    VARSERVER_HANDLE hVarServer;

    /*! verbose flag */
    bool verbose;

    /*! running flag */
    bool running;

    /*! Neurio sensor Address */
    char *address;

    /*! Neurio sensor URL */
    char *url;

    /*! Neurio sensor basic authentication */
    char *auth;

    /*! curl receive buffer */
    RxBuffer rxbuf;

    /*! Line 1 Voltage */
    VAR_HANDLE hL1V;

    /*! Line 1 Power */
    VAR_HANDLE hL1P;

    /*! Line 1 Reactive Power */
    VAR_HANDLE hL1Q;

    /*! Line 1 Energy Imported */
    VAR_HANDLE hL1EIn;

    /*! Line 2 Voltage */
    VAR_HANDLE hL2V;

    /*! Line 2 Power */
    VAR_HANDLE hL2P;

    /*! Line 2 Reactive Power */
    VAR_HANDLE hL2Q;

    /*! Line 2 Energy Imported */
    VAR_HANDLE hL2EIn;

    /*! Total Power */
    VAR_HANDLE hTotP;

    /*! Total Reactive Power */
    VAR_HANDLE hTotQ;

    /*! Total Energy Imported */
    VAR_HANDLE hTotEIn;

} NeurioState;

/*==============================================================================
        Private file scoped variables
==============================================================================*/

/*! MqttVars State object */
NeurioState state;

/*==============================================================================
        Private function declarations
==============================================================================*/

void main(int argc, char **argv);
static int ProcessOptions( int argC, char *argV[], NeurioState *pState );
static void usage( char *cmdname );
static int SetupVarHandles( NeurioState *pState );
static void SetupTerminationHandler( void );
static void TerminationHandler( int signum, siginfo_t *info, void *ptr );
static int QueryNeurio( NeurioState *pState );
static int InitReceiveBuffer( NeurioState *pState );
static size_t WriteMemoryCallback( void *contents,
                                   size_t size,
                                   size_t nmemb,
                                   void *userp );
static int NeurioStatus( NeurioState *pState, JNode *pNode );

/*==============================================================================
        Private function definitions
==============================================================================*/

/*============================================================================*/
/*  main                                                                      */
/*!
    Main entry point for the neurio application

    The main function starts the neurio application

    @param[in]
        argc
            number of arguments on the command line
            (including the command itself)

    @param[in]
        argv
            array of pointers to the command line arguments

    @return none

==============================================================================*/
void main(int argc, char **argv)
{
    VARSERVER_HANDLE hVarServer = NULL;
    VAR_HANDLE hVar;
    int result;
    JNode *neurio;
    JArray *vars;
    int sigval;
    int fd;
    int sig;
    int rc;
    char buf[BUFSIZ];

    /* clear the neurio state object */
    memset( &state, 0, sizeof( state ) );

    /* initialize the neurio state object */
    state.address = ADDRESS;

    if( argc < 2 )
    {
        usage( argv[0] );
        exit( 1 );
    }

    /* set up an abnormal termination handler */
    SetupTerminationHandler();

    /* process the command line options */
    ProcessOptions( argc, argv, &state );

    /* get the Neurio status url */
    rc = asprintf( &state.url,
                   "http://%s/current-sample",
                   state.address );

    if ( rc > 0 )
    {
        state.running = true;

        /* get a handle to the VAR server */
        state.hVarServer = VARSERVER_Open();
        if( state.hVarServer != NULL )
        {
            SetupVarHandles( &state );

            while( state.running )
            {
                sleep(1);
                QueryNeurio(&state);

                neurio = JSON_ProcessBuffer( state.rxbuf.p );
                NeurioStatus( &state, neurio );
                JSON_Free(neurio);
            }

            /* close the variable server */
            VARSERVER_Close( state.hVarServer );
        }
    }
}


/*============================================================================*/
/*  usage                                                                     */
/*!
    Display the application usage

    The usage function dumps the application usage message
    to stderr.

    @param[in]
       cmdname
            pointer to the invoked command name

    @return none

==============================================================================*/
static void usage( char *cmdname )
{
    if( cmdname != NULL )
    {
        fprintf(stderr,
                "usage: %s [-v] [-h] [-u address] [-a basic auth]\n"
                "-v : verbose mode\n"
                "-h : display this help\n"
                "-u : neurio sensor IP address\n"
                "-a : neurio basic auth\n",
                cmdname );
    }
}

/*============================================================================*/
/*  ProcessOptions                                                            */
/*!
    Process the command line options

    The ProcessOptions function processes the command line options and
    populates the MqttVarState object

    @param[in]
        argC
            number of arguments
            (including the command itself)

    @param[in]
        argv
            array of pointers to the command line arguments

    @param[in]
        pState
            pointer to the MqttVars state object

    @return none

==============================================================================*/
static int ProcessOptions( int argC, char *argV[], NeurioState *pState )
{
    int c;
    int result = EINVAL;
    const char *options = "hvu:a:";

    if( ( pState != NULL ) &&
        ( argV != NULL ) )
    {
        while( ( c = getopt( argC, argV, options ) ) != -1 )
        {
            switch( c )
            {
                case 'v':
                    pState->verbose = true;
                    break;

                case 'u':
                    pState->address = optarg;
                    break;

                case 'a':
                    pState->auth = optarg;
                    break;

                case 'h':
                    usage( argV[0] );
                    exit(1);
                    break;

                default:
                    break;

            }
        }
    }

    return 0;
}


/*============================================================================*/
/*  SetupTerminationHandler                                                   */
/*!
    Set up an abnormal termination handler

    The SetupTerminationHandler function registers a termination handler
    function with the kernel in case of an abnormal termination of this
    process.

==============================================================================*/
static void SetupTerminationHandler( void )
{
    static struct sigaction sigact;

    memset( &sigact, 0, sizeof(sigact) );

    sigact.sa_sigaction = TerminationHandler;
    sigact.sa_flags = SA_SIGINFO;

    sigaction( SIGTERM, &sigact, NULL );
    sigaction( SIGINT, &sigact, NULL );

}

/*============================================================================*/
/*  TerminationHandler                                                        */
/*!
    Abnormal termination handler

    The TerminationHandler function will be invoked in case of an abnormal
    termination of this process.  The termination handler closes
    the connection with the variable server and cleans up its VARFP shared
    memory.

@param[in]
    signum
        The signal which caused the abnormal termination (unused)

@param[in]
    info
        pointer to a siginfo_t object (unused)

@param[in]
    ptr
        signal context information (ucontext_t) (unused)

==============================================================================*/
static void TerminationHandler( int signum, siginfo_t *info, void *ptr )
{
    syslog( LOG_ERR, "Abnormal termination of neurio\n" );
    state.running = false;
}

/*============================================================================*/
/*  SetupVarHandles                                                           */
/*!
    Set up the Neurio variable handles

    The SetupVarHandles function sets up the variable handles for the
    voltage, power, and energy readings from the Neurio sensor

@param[in]
    pState
        pointer to the NeurioState object

==============================================================================*/
static int SetupVarHandles( NeurioState *pState )
{
    int result = EINVAL;

    if ( pState != NULL )
    {
        result = EOK;

        /* Line 1 Voltage */
        pState->hL1V = VAR_FindByName( pState->hVarServer,
                                       "/CONSUMPTION/L1/V" );

        /* Line 1 Power */
        pState->hL1P = VAR_FindByName( pState->hVarServer,
                                       "/CONSUMPTION/L1/P" );


        /* Line 1 Reactive Power */
        pState->hL1Q = VAR_FindByName( pState->hVarServer,
                                       "/CONSUMPTION/L1/Q" );

        /* Line 1 Energy Imported */
        pState->hL1EIn = VAR_FindByName( pState->hVarServer,
                                         "/CONSUMPTION/L1/ENERGY_IMP" );

        /* Line 2 Voltage */
        pState->hL2V = VAR_FindByName( pState->hVarServer,
                                       "/CONSUMPTION/L2/V" );

        /* Line 2 Power */
        pState->hL2P = VAR_FindByName( pState->hVarServer,
                                       "/CONSUMPTION/L2/P" );

        /* Line 2 Reactive Power */
        pState->hL2Q = VAR_FindByName( pState->hVarServer,
                                       "/CONSUMPTION/L2/Q" );

        /* Line 2 Energy Imported */
        pState->hL2EIn = VAR_FindByName( pState->hVarServer,
                                         "/CONSUMPTION/L2/ENERGY_IMP" );

        /* Total Power */
        pState->hTotP = VAR_FindByName( pState->hVarServer,
                                        "/CONSUMPTION/TOTAL/P" );

        /* Total Reactive Power */
        pState->hTotQ = VAR_FindByName( pState->hVarServer,
                                        "/CONSUMPTION/TOTAL/Q" );

        /* Total Energy Imported */
        pState->hTotEIn = VAR_FindByName( pState->hVarServer,
                                          "/CONSUMPTION/TOTAL/ENERGY_IMP" );
    }

    return result;
}

/*============================================================================*/
/*  QueryNeurio                                                               */
/*!
    Query the Nerio CT sensor

    The QueryNeurio function makes an http request to the Nerio CT
    sensor to get the current sensor state.

@param[in]
    pState
        pointer to the NeurioState object

==============================================================================*/
static int QueryNeurio( NeurioState *pState )
{
    CURL *curl;
    CURLcode res;
    struct curl_slist *headers = NULL;
    char auth[BUFSIZ];

    if ( pState != NULL )
    {
        /* clear the receive buffer */
        InitReceiveBuffer( pState );

        /* initialize curl */
        curl_global_init( CURL_GLOBAL_ALL );

        /* set up basic auth */
        snprintf( auth, BUFSIZ, "Authorization: Basic %s", pState->auth );

        curl = curl_easy_init();
        if (curl)
        {
            /* set the callback function */
            curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

            /* set the callback context */
            curl_easy_setopt( curl, CURLOPT_WRITEDATA, (void *)pState );

            /* set the address */
            curl_easy_setopt(curl, CURLOPT_URL, pState->url);

            /* add the authentication header */
            headers = curl_slist_append( headers, auth );

            /* set the headers */
            curl_easy_setopt( curl, CURLOPT_HTTPHEADER, headers );

            /* enable verbose output */
            curl_easy_setopt( curl, CURLOPT_VERBOSE, 0L );

            /* Perform the request, res will get the return code */
            res = curl_easy_perform( curl );

            /* Check for errors */
            if ( res != CURLE_OK )
            {
                fprintf(stderr, "curl_easy_perform() failed: %s\n",
                      curl_easy_strerror(res));
            }
            else
            {
                if ( pState->verbose )
                {
                    printf("%s\n", pState->rxbuf.p );
                }
            }

            /* always cleanup */
            curl_easy_cleanup( curl );

            /* free the custom headers */
            curl_slist_free_all( headers );

        }
    }

    return res;
}

/*============================================================================*/
/*  InitReceiveBuffer                                                         */
/*!
    Initialize the receive buffer

    The InitReceiveBuffer function initializes the receive buffer
    ready for a new curl transaction.  All indices and data sizes are
    reset to 0 ready for the new received data.

@param[in]
    pState
        pointer to the Neurio State object containing the receive buffer


@retval EOK the receive buffer was successfully initialized
@retval EINVAL invalid arguments

==============================================================================*/
static int InitReceiveBuffer( NeurioState *pState )
{
    int result = EINVAL;

    if ( pState != NULL )
    {
        if( pState->rxbuf.p != NULL )
        {
            /* clear the receive buffer */
            memset( pState->rxbuf.p, 0, pState->rxbuf.size );
        }
        else
        {
            /* set the buffer size to zero */
            pState->rxbuf.size = 0;
        }

        /* set the remaining buffer size */
        pState->rxbuf.remaining = pState->rxbuf.size;

        /* clear the received data length to zero */
        pState->rxbuf.len = 0;

        result = EOK;
    }

    return result;
}

/*============================================================================*/
/*  WriteMemoryCallback                                                       */
/*!
    Curl Write callback function

    The WriteMemoryCallback function is called by the curl library when
    a chunk of new data is available.  The new data is appended to the
    receive buffer at the current write point.  If there is not enough
    memory in the receive buffer, the buffer will be reallocated to
    create more memory.

@param[in]
    contents
        pointer to a received data chunk

@param[in]
    size
        received data chunk size

@param[in]
    nmemb
        number of received data chunks

@param[in]
    userp
        user context which points to the NeurioState object

==============================================================================*/
static size_t WriteMemoryCallback( void *contents,
                                   size_t size,
                                   size_t nmemb,
                                   void *userp )
{
    NeurioState *pState = (NeurioState *)userp;
    size_t realsize = 0;
    char *ptr;
    size_t offset;

    if ( ( pState != NULL ) && ( contents != NULL ) )
    {
        realsize = size * nmemb;

        if ( realsize > pState->rxbuf.remaining )
        {
            /* not enough space in the buffer, we need to reallocate */
            ptr = realloc( pState->rxbuf.p, pState->rxbuf.size + realsize + 1 );
            if ( !ptr )
            {
                /* out of memory */
                return 0;
            }

            /* update the rx buffer pointer */
            pState->rxbuf.p = ptr;

            /* update the total size and remaining bytes in the buffer */
            pState->rxbuf.size += ( realsize + 1 );
            pState->rxbuf.remaining += ( realsize + 1 );
        }

        /* get the write offset */
        offset = pState->rxbuf.len;

        /* append the received data */
        memcpy( &(pState->rxbuf.p[offset]), contents, realsize );

        /* calculate the new write offset */
        pState->rxbuf.remaining -= realsize;
        pState->rxbuf.len += realsize;

        /* NUL terminate */
        offset = pState->rxbuf.len;
        pState->rxbuf.p[offset] = 0;

    }

    return realsize;
}

/*============================================================================*/
/*  NeurioStatus                                                              */
/*!
    Handle the Neurio Status object

    The NeurioStatus function inspects the neurio status JSON object
    and extracts the line1 and line2 voltage and power consumption
    information.

@param[in]
    pState
        pointer to the NeurioState object

@param[in]
    pNode
        pointer to the Neurio Status JSON object


@retval EOK - the data was extracted successfully
@retval EINVAL - invalid argument

==============================================================================*/
static int NeurioStatus( NeurioState *pState, JNode *pNode )
{
    int result = EINVAL;
    JArray *channels;
    char *pSensorId;

    if ( ( pState != NULL ) && ( pNode != NULL ) )
    {
        pSensorId = JSON_GetStr( pNode, "sensorId" );

        /* get the channel information */
        channels = (JArray *)JSON_Find( pNode, "channels" );

        /* Line 1 data */
        pNode = JSON_Index( channels, 0 );

        /* Line 1 Real Power */
        VAR_Set( pState->hVarServer,
                 pState->hL1P,
                 JSON_GetVar( pNode, "p_W" ) );

        /* Line 1 Reactive Power */
        VAR_Set( pState->hVarServer,
                 pState->hL1Q,
                 JSON_GetVar( pNode, "q_VAR" ) );

        /* Line 1 Voltage */
        VAR_Set( pState->hVarServer,
                 pState->hL1V,
                 JSON_GetVar( pNode, "v_V" ) );

        /* Line 1 Consumption in Watt-Seconds */
        VAR_Set( pState->hVarServer,
                 pState->hL1EIn,
                 JSON_GetVar( pNode, "eImp_Ws" ) );

        /* Line 2 Data */

        pNode = JSON_Index( channels, 1 );

        /* Line 2 Real Power */
        VAR_Set( pState->hVarServer,
                 pState->hL2P,
                 JSON_GetVar( pNode, "p_W" ) );

        /* Line 2 Reactive Power */
        VAR_Set( pState->hVarServer,
                 pState->hL2Q,
                 JSON_GetVar( pNode, "q_VAR" ) );

        /* Line 2 Voltage */
        VAR_Set( pState->hVarServer,
                 pState->hL2V,
                 JSON_GetVar( pNode, "v_V" ) );

        /* Line 2 Consumption in Watt-Seconds */
        VAR_Set( pState->hVarServer,
                 pState->hL2EIn,
                 JSON_GetVar( pNode, "eImp_Ws" ) );

        /* Total Power */

        pNode = JSON_Index( channels, 2 );

        /* Total Real Power */
        VAR_Set( pState->hVarServer,
                 pState->hTotP,
                 JSON_GetVar( pNode, "p_W" ) );

        /* Total Reactive Power */
        VAR_Set( pState->hVarServer,
                 pState->hTotQ,
                 JSON_GetVar( pNode, "q_VAR" ) );

        /* Total Consumption in Watt-Seconds */
        VAR_Set( pState->hVarServer,
                 pState->hTotEIn,
                 JSON_GetVar( pNode, "eImp_Ws" ) );

        result = EOK;
    }

    return result;
}

/*! @}
 * end of neurio group */
