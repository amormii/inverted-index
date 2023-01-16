#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <unordered_set>
#include <chrono>

using namespace std::chrono;


// Структура даних для зберігання інвертованого індексу
std::unordered_map<std::string, std::unordered_set<int>> invertedIndex;
std::mutex indexMutex; // М'ютекс для захисту доступу до індексу

// Функція для розділення документу на окремі слова
std::vector<std::string> splitDocIntoWords(const std::string& document){
    std::istringstream oss(document);
    std::string word;
    std::vector<std::string> splitString;

    while (oss >> word){
        auto it = std::remove_if(word.begin(), word.end(), ::ispunct); // Читаємо слово, видаляємо розділові знаки
        word.erase(it, word.end());
        splitString.push_back(word); // Додаємо слово у вектор розділених слів
    }
    return splitString;
}

// Функція, яка будує інвертований індекс для документів у вказаному проміжку
void buildInvertedIndex(const std::vector<std::string>& documents, const int start, const int end) {
    for (int i = start; i < end; i++) { // Ітеруємось по документам у вказаному проміжку
        std::vector<std::string> words = splitDocIntoWords(documents[i]); // Розділяємо документи на слова
        for (const std::string& word : words) {
            std::unique_lock<std::mutex> lock(indexMutex);

            if (invertedIndex.find(word) == invertedIndex.end()) {
                invertedIndex[word] = std::unordered_set<int>(); // Якщо слова ще немає в індексі, додаємо його
            }
            // Додаємо індекс документа в множину документів, які містять це слово
            invertedIndex[word].insert(i);
            lock.unlock();
        }
    }
}

// Функція, яка приблизно рівномірно розподіляє документи між потоками
void threadBuildInvertedIndex(const std::vector<std::string>& documents, int numThreads) {
    int chunkSize = documents.size() / numThreads;     // Ділимо документи на частини для кожного потоку
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; i++) {
        int start = i * chunkSize;
        int end = (i + 1) * chunkSize;
        if (i == numThreads - 1) {
            end = documents.size(); // Для останнього потоку виставляємо кінцевою межею кінець вектору
        }
        threads.emplace_back(buildInvertedIndex, std::ref(documents), start, end); // Для кожного потоку викликаємо функцію
    }
    // Чекаємо на завершення всіх потоків
    for (std::thread& t : threads) {
        t.join();
    }
}


// Функція, яка знаходить інвертований індекс для вказаного слова
std::unordered_set<int> searchInvertedIndex(const std::string& word) {
    //Перевіряє, чи є слово в індексі
    if (invertedIndex.find(word) != invertedIndex.end()) {
        return invertedIndex[word]; // Якщо слово знайшлося, повертає множину документів, в яких воно зустрічається
    } else {
        return {}; // Якщо слова немає, функція повертає порожню множину
    }
}

// Функція, яка читає документи з директорії та додає їх у вектор
std::vector<std::string> readFiles(std::ifstream& fin, const std::filesystem::path& p) {
    std::vector<std::string> readDocuments;
    for (auto& file : std::filesystem::directory_iterator(p)){ // Кожен файл в директорії відкриваємо та читаємо його текст, додаємо у вектор
        fin.open(file.path(), std::ios_base::in);
        std::string doc;
        while (!fin.eof()){
            std::getline(fin, doc);
        }
        readDocuments.push_back(doc);
        fin.close();
    }

    return readDocuments;
}

int main (){
    const int numThreads = 8;

    std::ifstream fin;
    std::vector<std::string> documents{};
    std::unordered_set<int> searchResults;
    const std::filesystem::path p = std::filesystem::current_path() / std::filesystem::path("aclImdb"); // Задаємо стандартний шлях для пошуку файлів

    documents = readFiles(fin, p); // Читаємо документи за вказаним шляхом

    std::cout << "Inverted index is building...\n";
    const auto buildIndexStart = system_clock::now();

    threadBuildInvertedIndex(documents, numThreads); // Будуємо інвертований індекс з вказаною кількістю потоків

    const auto buildIndexEnd = system_clock::now();
    std::cout << "Inverted index built in " << duration_cast<milliseconds> (buildIndexEnd - buildIndexStart) << '\n';

    std::cout << "Enter a word you want to search in the documents.\n";
    std::string word;
    std::cin >> word;

    auto searchIndexStart = system_clock::now();
    searchResults = searchInvertedIndex(word); // Шукаємо введене слово серед інвертованого індексу
    auto searchIndexEnd = system_clock::now();

    std::cout << "Search took " << duration_cast<microseconds> (searchIndexEnd - searchIndexStart) << '\n';


    if (searchResults.empty()) {
        std::cout << "Unfortunately, this word hasn't been found among the documents.";
    }
    else{
        for (const int& x : searchResults){
            std::cout << "\"" << word << "\" found in the file with index: " << x << '\n'; // Для кожного знайденого слова виводимо індекс файлу, в якому воно було знайдене
        }
    }

    return 0;
}