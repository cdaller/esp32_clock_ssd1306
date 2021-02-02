#ifndef JsonFetchData_hpp
#define JsonFetchData_hpp

//#define DEFAULT_FORMATTER "%.1f"

class JsonFetchData {
    public:
        JsonFetchData(char* jsonUrl, char* jsonPath, long fetchInvervalMs);
        ~JsonFetchData() {}

        float getValue();
        char* getValueAsString();
        int getStatus() { return lastStatus; }
        const char* getStatusAsText();
        void resetFetchInterval() { lastFetchValueMillis = 0; };

        //void setFormatter(const char* formatter) { this-> formatter = formatter; };


        static const int STATUS_DATA_OK = 0;
        static const int STATUS_JSON_PARSE_ERROR = 1;
        static const int STATUS_HTTP_ERROR = 2;
        static const int STATUS_NO_DATA = 3;

    private:
        char* formatter; // = DEFAULT_FORMATTER;
        char* jsonUrl;
        char* jsonPath;
        long fetchIntervalMs;
        long lastFetchValueMillis;
        float lastValue;
        int lastStatus = STATUS_NO_DATA;

        void fetchJsonValue();
        void parseJson(char* jsonString, char *jsonPath);
        
};

#endif // JsonFetchData_hpp