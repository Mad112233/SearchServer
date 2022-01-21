#pragma once
#include "document.h"
#include "string_processing.h"

#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <iostream>
#include <execution>
#include <string_view>
#include <deque>
#include <mutex>

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double RATE = 1e-6;
const int BUCKET_COUNT = 5;

using namespace std::string_literals;

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words) : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
    {
        if (!std::all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
            throw std::invalid_argument("Some of stop words are invalid");
        }
    }

    explicit SearchServer(const std::string& stop_words_text);
    void AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const {
        const auto query = ParseQuery(raw_query);

        auto matched_documents = FindAllDocuments(query, document_predicate);

        std::sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < RATE) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }

        return matched_documents;
    }

    template <typename DocumentPredicate, typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy& policy, const std::string_view raw_query, DocumentPredicate document_predicate) const {
        const auto query = ParseQuery(raw_query);
        std::vector<Document> matched_documents;

        if (std::is_same_v<ExecutionPolicy, std::execution::sequenced_policy>)
            matched_documents = FindAllDocuments(query, document_predicate);
        else
            matched_documents = FindAllDocuments(std::execution::par, query, document_predicate);

        std::sort(std::execution::par, matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < RATE) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }

        return matched_documents;
    }

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy& policy, const std::string_view raw_query, DocumentStatus status) const {
        return FindTopDocuments(policy, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
            });
    }

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy& policy, const std::string_view raw_query) const {
        return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
    }

    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::string_view raw_query) const;
    int GetDocumentCount() const;
    std::set<int>::iterator begin()const;
    std::set<int>::iterator end()const;
    const std::map<std::string, double>& GetWordFrequencies(int document_id) const;
    void RemoveDocument(int document_id);

    template <class ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy& policy, int document_id) {
        RemoveDocument(document_id);
    }

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view raw_query, int document_id) const;

    template <class ExecutionPolicy>
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(ExecutionPolicy& policy, const std::string_view raw_query, int document_id) const {
        if (document_ids_.count(document_id) == 0) {
            throw std::out_of_range("ID нет!");
        }

        const auto query = ParseQuery(raw_query);

        std::vector<std::string_view> matched_words;
        std::mutex mut;

        std::for_each(policy, query.plus_words.begin(), query.plus_words.end(), [this, document_id, &matched_words, &mut](const std::string& word) {
            if (word_to_document_freqs_.count(word) && word_to_document_freqs_.at(word).count(document_id))
                std::lock_guard guard(mut);
            matched_words.push_back(word_to_document_freqs_.find(word)->first);
            });

        std::for_each(policy, query.minus_words.begin(), query.minus_words.end(), [this, document_id, &matched_words, &mut](const std::string& word) {
            if (word_to_document_freqs_.count(word) && word_to_document_freqs_.at(word).count(document_id)) {
                std::lock_guard guard(mut);
                matched_words.clear();
                return;
            }
            });

        return { matched_words, documents_.at(document_id).status };
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    std::map<std::string, double> empty_freq;
    std::map<int, std::map<std::string, double>> id_word_frequencies_;
    const std::set<std::string> stop_words_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;
    bool IsStopWord(const std::string& word) const;
    static bool IsValidWord(const std::string& word);
    std::vector<std::string> SplitIntoWordsNoStop(const std::string_view text) const;
    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(const std::string& text) const;

    struct Query {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };

    Query ParseQuery(const std::string_view text) const;
    double ComputeWordInverseDocumentFreq(const std::string& word) const;

    template <typename Key, typename Value>
    class ConcurrentMap {
    public:
        static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

        ConcurrentMap() = default;

        struct Access {
            Access(std::map<Key, Value>& ref_map, const Key& key, std::mutex& mut) :mut_lock(mut), ref_to_map(ref_map), ref_key(key) {}

            std::lock_guard<std::mutex> mut_lock;
            std::map<Key, Value>& ref_to_map;
            const Key& ref_key;
            Value& ref_to_value = ref_to_map[ref_key];

            Value& operator+=(const Value& value) {
                ref_to_value += value;
                return ref_to_value;
            }
        };

        explicit ConcurrentMap(size_t bucket_count) {
            maps_.resize(bucket_count);
            mutexs_.resize(bucket_count);
        }

        int IDX(const Key& key) {
            const size_t size = maps_.size();
            return (key % size + size) % size;
        }

        Access operator[](const Key& key) {
            const int idx = IDX(key);
            return { maps_[idx],key, mutexs_[idx] };
        }

        void erase(const Key& key) {
            const int idx = IDX(key);
            Access(maps_[idx], key, mutexs_[idx]);
            maps_[idx].erase(key);
        }

        std::map<Key, Value> BuildOrdinaryMap() {
            std::map<Key, Value>answer;
            for (size_t i = 0; i < maps_.size(); ++i) {
                std::lock_guard mut(mutexs_[i]);
                answer.merge(maps_[i]);
            }
            return answer;
        }

        auto begin() {
            mut_.lock();
            answer_ = BuildOrdinaryMap();
            return answer_.begin();
        }

        auto end() {
            mut_.unlock();
            return answer_.end();
        }

    private:
        std::vector<std::map<Key, Value>>maps_;
        std::deque<std::mutex>mutexs_;
        std::mutex mut_;
        std::map<Key, Value> answer_;
    };

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy&, const Query& query, DocumentPredicate document_predicate) const {
        ConcurrentMap<int, double> document_to_relevance(BUCKET_COUNT);

        std::for_each(std::execution::par, query.plus_words.begin(), query.plus_words.end(),
            [this, &document_to_relevance, &document_predicate](const std::string& word) {
                if (word_to_document_freqs_.count(word)) {
                    const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                    for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                        if (documents_.count(document_id) == 0)
                            continue;
                        const auto& document_data = documents_.at(document_id);
                        if (document_predicate(document_id, document_data.status, document_data.rating)) {
                            document_to_relevance[document_id] += term_freq * inverse_document_freq;
                        }
                    }
                }
            });

        std::for_each(std::execution::par, query.minus_words.begin(), query.minus_words.end(),
            [this, &document_to_relevance](const std::string& word) {
                if (word_to_document_freqs_.count(word)) {
                    for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                        document_to_relevance.erase(document_id);
                    }
                }
            });

        std::vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        std::map<int, double> document_to_relevance;

        for (const std::string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                if (documents_.count(document_id) == 0)
                    continue;
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const std::string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        std::vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }
};