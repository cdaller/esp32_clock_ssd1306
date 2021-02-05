#ifndef JsonFetchData_hpp
#define JsonFetchData_hpp

#include <WString.h>

class JsonFetchData {
    public:
        JsonFetchData(char* jsonUrl, char* jsonPath, long fetchInvervalMs);
        ~JsonFetchData() {}

        float getValue();
        char* getValueFormatted();
        int getStatus();
        const char* getStatusAsText();
        void resetFetchInterval() { lastFetchValueMillis = 0; };

        void setFormatter(char* formatter);

        char DEFAULT_FORMATTER[10] = "%.1f";
        static const int STATUS_DATA_OK = 0;
        static const int STATUS_JSON_PARSE_ERROR = 1;
        static const int STATUS_HTTP_ERROR = 2;
        static const int STATUS_NO_DATA = 3;

    private:
        const char* TAG = "JSON_FETCH_DATA"; // debug tag
        String formatter;
        char* jsonUrl;
        char* jsonPath;
        long fetchIntervalMs;
        long lastFetchValueMillis;
        float lastValue;
        char lastValueFormatted[100];
        int lastStatus = STATUS_NO_DATA;

        void fetchValueIfNeeded();
        void fetchJsonValue();
        void parseJson(char* jsonString, char *jsonPath);
        
};

#endif // JsonFetchData_hpp