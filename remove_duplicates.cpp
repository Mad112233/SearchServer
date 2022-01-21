#include <set>
#include <string>
#include <map>
#include <iostream>
#include <string_view>

#include "search_server.h"
#include "remove_duplicates.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
    set<int> id_for_deletion;
    map<set<string_view>, int> documents_on_server;
    for (const int document_id : search_server) {
        set<string_view> words;
        for (auto& [word, freq] : search_server.GetWordFrequencies(document_id)) {
            words.insert(word);
        }
        const auto it = documents_on_server.find(words);
        if (it != documents_on_server.end()) {
            if (it->second < document_id) {
                id_for_deletion.insert(document_id);
            }
            else {
                id_for_deletion.insert(it->second);
                it->second = document_id;
            }
        }
        else {
            documents_on_server[words] = document_id;
        }
    }
    for (int id : id_for_deletion) {
        search_server.RemoveDocument(id);
        cout << "Found duplicate document id " << id << endl;
    }
}