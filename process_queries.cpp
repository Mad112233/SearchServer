#include "process_queries.h"
#include "search_server.h"
#include "document.h"

#include <algorithm>
#include <execution>

using namespace std;

vector<vector<Document>> ProcessQueries(const SearchServer& search_server, const vector<string>& queries) {
	vector<vector<Document>>array_top_documents(queries.size());
	transform(execution::par, queries.begin(), queries.end(), array_top_documents.begin(), [&search_server](const string& str) {
		return search_server.FindTopDocuments(execution::par, str);
		});
	return array_top_documents;
}

list<Document> ProcessQueriesJoined(const SearchServer& search_server, const vector<string>& queries) {
	list<Document>documents;
	for (const auto& arr : ProcessQueries(search_server, queries)) {
		documents.splice(documents.end(), { arr.begin(), arr.end() });
	}
	return documents;
}