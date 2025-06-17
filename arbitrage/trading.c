#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <unistd.h>

#define API_KEY ""
#define SYMBOL "AAPL"
#define INTERVAL 1  // seconds

#define DEFAULT_MOMENTUM 0.05
#define BUY_THRESH 1.005

#define HOLD 0
#define SCARE 1
#define PANIC 2

struct string {
    char *ptr;
    size_t len;
};

struct wallet{
    float money;
    float shares;
    float invstd;
    float momentum;
};

void buy(struct wallet* wlt, float price){
    float buy_amnt = 0;

    if(wlt->momentum < 0) wlt->momentum = DEFAULT_MOMENTUM;

    buy_amnt = (wlt->momentum);
    wlt->momentum *= 2;
    if(wlt->momentum > 1) wlt->momentum = 1;

    if(buy_amnt*price > wlt->money) buy_amnt = wlt->money/price;

    printf("buying %f\n", buy_amnt);

    wlt->shares += buy_amnt;
    wlt->invstd += buy_amnt*price;
    wlt->money  -= buy_amnt*price;

    printf("liquid cash: $%f, shares: %f, invested: $%f, net worth $%f \n", wlt->money, wlt->shares, wlt->invstd, wlt->money+(wlt->shares*price));

}

void sell(struct wallet* wlt, float price){

    float sell_amnt = 0;

    if(wlt->momentum > 0) wlt->momentum = -DEFAULT_MOMENTUM;

    sell_amnt = (wlt->shares) * (-(wlt->momentum));
    wlt->momentum *= 2;
    if(wlt->momentum < -1) wlt->momentum = -1;

    printf("selling $%f", sell_amnt);

    wlt->shares -= sell_amnt;
    wlt->invstd -= sell_amnt*price;
    wlt->money  += sell_amnt*price;

    printf("liquid cash: $%f, shares: %f, invested: $%f, net worth $%f \n", wlt->money, wlt->shares, wlt->invstd, wlt->money+(wlt->shares*price));

}

void cash_out(struct wallet* wlt, float price){

    float sell_amnt = wlt->shares;

    wlt->momentum = DEFAULT_MOMENTUM;

    printf("cashing out $%f", sell_amnt);

    wlt->shares -= sell_amnt;
    wlt->invstd -= sell_amnt*price;
    wlt->money  += sell_amnt*price;

    printf("liquid cash: $%f, shares: %f, invested: $%f, net worth $%f \n", wlt->money, wlt->shares, wlt->invstd, wlt->money+(wlt->shares*price));

}

void init_string(struct string *s) {
    s->len = 0;
    s->ptr = malloc(1);
    if (s->ptr == NULL) {
        fprintf(stderr, "malloc() failed\n");
        exit(EXIT_FAILURE);
    }
    s->ptr[0] = '\0';
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string *s) {
    size_t new_len = s->len + size * nmemb;
    s->ptr = realloc(s->ptr, new_len + 1);
    if (s->ptr == NULL) {
        fprintf(stderr, "realloc() failed\n");
        exit(EXIT_FAILURE);
    }
    memcpy(s->ptr + s->len, ptr, size * nmemb);
    s->ptr[new_len] = '\0';
    s->len = new_len;
    return size * nmemb;
}

float fetch_stock_price(const char *symbol) {
    CURL *curl;
    CURLcode res;
    float val = -1;
    char url[512];
    static float prev_price = 0;
    snprintf(url, sizeof(url),
             "https://finnhub.io/api/v1/quote?symbol=%s&token=%s",
             symbol, API_KEY);

    curl = curl_easy_init();
    if (curl) {
        struct string s;
        init_string(&s);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
        

        res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            //printf("->%s\n", s.ptr);
            char *price_str = strstr(s.ptr, "\"c\":");
            if (price_str) {
                float price;
                sscanf(price_str, "\"c\":%f", &price);
                if(prev_price != price){
                    printf("Current price of %s: $%f\n", symbol, price);
                    prev_price = price;
                }
                val = price;
            } else {
                printf("Could not parse price\n");
            }
        } else {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));

            
        }

        
        free(s.ptr);
        curl_easy_cleanup(curl);
    }

    return val;
}

void trade(float price, struct wallet* wlt){
    
    int next_state = HOLD;
    static float prev_price = 0;
    static float buy_price = 0;
    static int curr_state = 0;
    static float value = 1000;
    static int conf = 1;

    switch(curr_state){

        case(HOLD): 
            if(price < prev_price){
                next_state = SCARE;
                if(conf == 0) sell(wlt, price);
            }
            else{
                if(wlt->money < 5 && wlt->shares*price > value){
                    value = wlt->shares*price > value;
                    cash_out(wlt, price);
                }
                else if(price >= buy_price*BUY_THRESH){
                    buy_price = price;
                    buy(wlt, price);
                }
                next_state = HOLD;
            }
            conf = 1;
            break;

        case(SCARE):
            conf = 0;
            if(price < prev_price){ 
                next_state = PANIC;
                sell(wlt, price);
            }
            else{
                next_state = HOLD;
                buy(wlt, price);
            }
            break;

        case(PANIC):
            sell(wlt, price);

            if(price > prev_price) next_state = HOLD;
            else next_state = PANIC;
            break;
        default:
            break;
    }

    prev_price = price;
    curr_state = next_state;


}

int main() {
    float price;
    struct wallet wlt;

    wlt.money = 1000;
    wlt.shares = 0;
    wlt.invstd = 0;
    wlt.momentum = DEFAULT_MOMENTUM;

    while (1) {
        price = fetch_stock_price(SYMBOL);

        if(price > 0){
            trade(price, &wlt);
        }
        sleep(INTERVAL);
    }
    return 0;
}

