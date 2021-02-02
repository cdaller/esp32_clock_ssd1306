#ifndef JsonFetchData_hpp
#define JsonFetchData_hpp

class JsonFetchData {
    public:
        JsonFetchData(char* jsonUrl, char* jsonPath, long fetchInvervalMs);
        ~JsonFetchData() {}

        float getValue();
        char* getValueFormatted() { return lastValueFormatted; }
        int getStatus() { return lastStatus; }
        const char* getStatusAsText();
        void resetFetchInterval() { lastFetchValueMillis = 0; };

        void setFormatter(char* formatter) { this-> formatter = formatter; };

        char DEFAULT_FORMATTER[10] = "%.1f";
        static const int STATUS_DATA_OK = 0;
        static const int STATUS_JSON_PARSE_ERROR = 1;
        static const int STATUS_HTTP_ERROR = 2;
        static const int STATUS_NO_DATA = 3;

    private:
        char* formatter;
        char* jsonUrl;
        char* jsonPath;
        long fetchIntervalMs;
        long lastFetchValueMillis;
        float lastValue;
        char lastValueFormatted[100];
        int lastStatus = STATUS_NO_DATA;

        void fetchJsonValue();
        void parseJson(char* jsonString, char *jsonPath);
        
};

#endif // JsonFetchData_hpp