#pragma once
#include "search_server.h"
#include "document.h"

#include <string>
#include <vector>
#include <deque>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        const std::vector<Document>documents = server.FindTopDocuments(raw_query, document_predicate);
        ++time;
        if (time == sec_in_day_) {
            requests_.pop_front();
            --time;
        }
        const int value = documents.empty() ? 1 : 0;
        if (!requests_.empty()) {
            requests_.push_back({ requests_.back().count_no_result + value,raw_query });
        }
        else {
            requests_.push_back({ value,raw_query });
        }
        return documents;
    }

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string& raw_query);
    int GetNoResultRequests() const;

private:
    struct QueryResult {
        int count_no_result = 0;
        std::string query;
    };
    std::deque<QueryResult> requests_;
    const static int sec_in_day_ = 1440;
    int time = 0;
    const SearchServer& server;
};