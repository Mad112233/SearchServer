#pragma once

#include <string>
#include <vector>
#include <string_view>

struct Document {
    Document() = default;

    Document(int id, double relevance, int rating);

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer;

void PrintDocument(const Document& document);
void PrintMatchDocumentResult(int document_id, const std::vector<std::string_view>& words, DocumentStatus status);
void AddDocument(SearchServer& search_server, int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings);
void FindTopDocuments(const SearchServer& search_server, const std::string_view raw_query);
void MatchDocuments(const SearchServer& search_server, const std::string_view query);