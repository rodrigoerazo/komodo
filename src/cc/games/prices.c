
#include "prices.h"
#include <time.h>
#include <unistd.h>
#ifdef BUILD_GAMESCC
#include "../rogue/cursesd.h"
#else
#include <curses.h>
#endif

#define SATOSHIDEN ((uint64_t)100000000L)
#define issue_curl(cmdstr) bitcoind_RPC(0,(char *)"prices",cmdstr,0,0,0)
extern int64_t Net_change;

/*
 In order to port a game into gamesCC, the RNG needs to be seeded with the gametxid seed, also events needs to be broadcast using issue_games_events. Also the game engine needs to be daemonized, preferably by putting all globals into a single data structure.
 
 also, the standalone game needs to support argv of seed gametxid, along with replay args
 */

int random_tetromino(struct games_state *rs)
{
    rs->seed = _games_rngnext(rs->seed);
    return(rs->seed % NUM_TETROMINOS);
}

int32_t pricesdata(struct games_player *P,void *ptr)
{
    tetris_game *tg = (tetris_game *)ptr;
    P->gold = tg->points;
    P->dungeonlevel = tg->level;
    //fprintf(stderr,"score.%d level.%d\n",tg->points,tg->level);
    return(0);
}

void sleep_milli(int milliseconds)
{
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = milliseconds * 1000 * 1000;
    nanosleep(&ts, NULL);
}

struct games_state globalR;
extern char Gametxidstr[];
int32_t issue_games_events(struct games_state *rs,char *gametxidstr,uint32_t eventid,gamesevent c);
uint64_t get_btcusd();

void *gamesiterate(struct games_state *rs)
{
    bool running = true; uint32_t eventid = 0; int64_t price;
    if ( rs->guiflag != 0 || rs->sleeptime != 0 )
    {
    }
    while ( running != 0 )
    {
        //running = tg_tick(rs,tg,move);
        if ( rs->guiflag != 0 || rs->sleeptime != 0 )
        {
        }
        if ( rs->guiflag != 0 )
        {
#ifdef STANDALONE
            price = get_btcusd();
            //fprintf(stderr,"%llu -> t%u %.4f\n",(long long)price,(uint32_t)(price >> 32),(double)(price & 0xffffffff)/10000);
            issue_games_events(rs,Gametxidstr,eventid,price);
            eventid++;
            sleep(10);
            switch ( getch() )
            {
                case '+': Net_change++; break;
                case '-': Net_change--; break;
            }
            /*if ( (counter++ % 10) == 0 )
                doupdate();
            c = games_readevent(rs);
            if ( c <= 0x7f || skipcount == 0x3fff )
            {
                if ( skipcount > 0 )
                    issue_games_events(rs,Gametxidstr,eventid-skipcount,skipcount | 0x4000);
                if ( c <= 0x7f )
                    issue_games_events(rs,Gametxidstr,eventid,c);
                if ( tg->level != prevlevel )
                {
                    flushkeystrokes(rs,0);
                    prevlevel = tg->level;
                }
                skipcount = 0;
            } else skipcount++;*/
#endif
        }
        else
        {
            if ( rs->replaydone != 0 )
                break;
            if ( rs->sleeptime != 0 )
            {
                sleep_milli(1);
            }
            /*if ( skipcount == 0 )
            {
                c = games_readevent(rs);
                //fprintf(stderr,"%04x score.%d level.%d\n",c,tg->points,tg->level);
                if ( (c & 0x4000) == 0x4000 )
                {
                    skipcount = (c & 0x3fff);
                    c = 'S';
                }
            }
            if ( skipcount > 0 )
                skipcount--;*/
        }
        eventid++;
    }
    return(0);
}

#ifdef STANDALONE
#include <ncurses.h>
#include "dapps/dappstd.c"
int64_t Net_change;

char *send_curl(char *url,char *fname)
{
    char *retstr;
    retstr = issue_curl(url);
    return(retstr);
}

cJSON *get_urljson(char *url,char *fname)
{
    char *jsonstr; cJSON *json = 0;
    if ( (jsonstr= send_curl(url,fname)) != 0 )
    {
        //printf("(%s) -> (%s)\n",url,jsonstr);
        json = cJSON_Parse(jsonstr);
        free(jsonstr);
    }
    return(json);
}

//////////////////////////////////////////////
// start of dapp
//////////////////////////////////////////////

uint64_t get_btcusd()
{
    cJSON *pjson,*bpi,*usd; uint64_t x,newprice,mult,btcusd = 0;
    if ( (pjson= get_urljson((char *)"http://api.coindesk.com/v1/bpi/currentprice.json",(char *)"/tmp/oraclefeed.json")) != 0 )
    {
        if ( (bpi= jobj(pjson,(char *)"bpi")) != 0 && (usd= jobj(bpi,(char *)"USD")) != 0 )
        {
            btcusd = jdouble(usd,(char *)"rate_float") * SATOSHIDEN;
            mult = 10000 + Net_change*10;
            newprice = (btcusd * mult) / 10000;
            x = ((uint64_t)time(NULL) << 32) | ((newprice / 10000) & 0xffffffff);
            printf("BTC/USD %.4f Net_change %lld * 0.001 -> %.4f\n",dstr(btcusd),(long long)Net_change,dstr(newprice));
        }
        free_json(pjson);
    }
    return(x);
}

char *clonestr(char *str)
{
    char *clone; int32_t len;
    if ( str == 0 || str[0] == 0 )
    {
        printf("warning cloning nullstr.%p\n",str);
#ifdef __APPLE__
        while ( 1 ) sleep(1);
#endif
        str = (char *)"<nullstr>";
    }
    len = strlen(str);
    clone = (char *)calloc(1,len+16);
    strcpy(clone,str);
    return(clone);
}

int32_t issue_games_events(struct games_state *rs,char *gametxidstr,uint32_t eventid,gamesevent c)
{
    static FILE *fp;
    char params[512],*retstr; cJSON *retjson,*resobj; int32_t retval = -1;
    if ( fp == 0 )
        fp = fopen("events.log","wb");
    rs->buffered[rs->num++] = c;
    if ( 1 )
    {
        if ( sizeof(c) == 1 )
            sprintf(params,"[\"events\",\"17\",\"[%%22%02x%%22,%%22%s%%22,%u]\"]",(uint8_t)c&0xff,gametxidstr,eventid);
        else if ( sizeof(c) == 2 )
            sprintf(params,"[\"events\",\"17\",\"[%%22%04x%%22,%%22%s%%22,%u]\"]",(uint16_t)c&0xffff,gametxidstr,eventid);
        else if ( sizeof(c) == 4 )
            sprintf(params,"[\"events\",\"17\",\"[%%22%08x%%22,%%22%s%%22,%u]\"]",(uint32_t)c&0xffffffff,gametxidstr,eventid);
        else if ( sizeof(c) == 8 )
            sprintf(params,"[\"events\",\"17\",\"[%%22%016llx%%22,%%22%s%%22,%u]\"]",(long long)c,gametxidstr,eventid);
        if ( (retstr= komodo_issuemethod(USERPASS,(char *)"cclib",params,GAMES_PORT)) != 0 )
        {
            if ( (retjson= cJSON_Parse(retstr)) != 0 )
            {
                if ( (resobj= jobj(retjson,(char *)"result")) != 0 )
                {
                    retval = 0;
                    if ( fp != 0 )
                    {
                        fprintf(fp,"%s\n",jprint(resobj,0));
                        fflush(fp);
                    }
                }
                free_json(retjson);
            } else fprintf(fp,"error parsing %s\n",retstr);
            free(retstr);
        } else fprintf(fp,"error issuing method %s\n",params);
        return(retval);
    } else return(0);
}

int prices(int argc, char **argv)
{
    struct games_state *rs = &globalR;
    int32_t c,skipcount=0; uint32_t eventid = 0;
    memset(rs,0,sizeof(*rs));
    rs->guiflag = 1;
    rs->sleeptime = 1; // non-zero to allow refresh()
    if ( argc >= 2 && strlen(argv[2]) == 64 )
    {
#ifdef _WIN32
#ifdef _MSC_VER
        rs->origseed = _strtoui64(argv[1], NULL, 10);
#else
        rs->origseed = atol(argv[1]); // windows, but not MSVC
#endif // _MSC_VER
#else
        rs->origseed = atol(argv[1]); // non-windows
#endif // _WIN32
        rs->seed = rs->origseed;
        if ( argc >= 3 )
        {
            strcpy(Gametxidstr,argv[2]);
            fprintf(stderr,"setplayerdata %s\n",Gametxidstr);
            if ( games_setplayerdata(rs,Gametxidstr) < 0 )
            {
                fprintf(stderr,"invalid gametxid, or already started\n");
                return(-1);
            }
        }
    } else rs->seed = 777;
    gamesiterate(rs);
    //gamesbailout(rs);
    return 0;
}

#endif
