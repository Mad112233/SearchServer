#include "request_queue.h"
#include "document.h"
#include "search_server.h"

#include <vector>
#include <string>

using namespace std;

RequestQueue::RequestQueue(const SearchServer& search_server) :server(search_server) {
}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentStatus status) {
    return AddFindRequest(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
        });
}
vector<Document> RequestQueue::AddFindRequest(const string& raw_query) {
    return AddFindRequest(raw_query, DocumentStatus::ACTUAL);
}

int RequestQueue::GetNoResultRequests() const {
    if (requests_.empty())
        return 0;
    else
        return requests_.back().count_no_result - requests_.front().count_no_result + 2;
}