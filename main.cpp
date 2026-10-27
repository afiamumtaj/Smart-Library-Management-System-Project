#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <limits>
#include <iomanip>
#include <ctime>
#include <cstdio>
#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

const double FINE_PER_DAY = 5.0;
const string BOOK_FILE = "books.txt";
const string MEMBER_FILE = "members.txt";
const string TRANSACTION_FILE = "transactions.txt";

// ANSI colours make the console menu easier to read on supported terminals.
const string RESET = "\033[0m";
const string RED = "\033[31m";
const string GREEN = "\033[32m";
const string YELLOW = "\033[33m";
const string BLUE = "\033[34m";
const string CYAN = "\033[36m";
const string BOLD = "\033[1m";

void enableConsoleColors() {
#ifdef _WIN32
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(output, &mode))
            SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif
}

// Parse and validate a date in DD-MM-YYYY format.
bool parseDate(const string& text, tm& parsed) {
    int day, month, year;
    char dash1, dash2;
    stringstream ss(text);
    if (!(ss >> day >> dash1 >> month >> dash2 >> year) ||
        dash1 != '-' || dash2 != '-' || !ss.eof()) return false;
    if (year < 1900 || month < 1 || month > 12 || day < 1 || day > 31) return false;
    tm candidate = {};
    candidate.tm_mday = day;
    candidate.tm_mon = month - 1;
    candidate.tm_year = year - 1900;
    candidate.tm_hour = 12;
    candidate.tm_isdst = -1;
    time_t value = mktime(&candidate);
    if (value == static_cast<time_t>(-1)) return false;
    tm checked = {};
#ifdef _WIN32
    localtime_s(&checked, &value);
#else
    localtime_r(&value, &checked);
#endif
    if (checked.tm_mday != day || checked.tm_mon != month - 1 || checked.tm_year != year - 1900)
        return false;
    parsed = checked;
    return true;
}

// Returns the signed number of calendar days from start to end.
string todayDate() {
    time_t now = time(nullptr);
    tm local = {};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%02d-%02d-%04d", local.tm_mday,
             local.tm_mon + 1, local.tm_year + 1900);
    return buffer;
}

bool calendarDaysBetween(const string& start, const string& end, int& days) {
    tm a = {}, b = {};
    if (!parseDate(start, a) || !parseDate(end, b)) return false;
    a.tm_hour = b.tm_hour = 12;
    time_t ta = mktime(&a), tb = mktime(&b);
    if (ta == static_cast<time_t>(-1) || tb == static_cast<time_t>(-1)) return false;
    days = static_cast<int>((tb - ta) / (60 * 60 * 24));
    return true;
}

int readInt(const string& prompt) {
    int value;
    while (true) {
        cout << prompt;
        if (cin >> value) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return value;
        }
        cout << "Invalid input. Please enter a number.\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

string readText(const string& prompt) {
    string value;
    cout << prompt;
    getline(cin, value);
    return value;
}

struct Book {
    int id{};
    string title;
    string author;
    int totalCopies{};
};

struct Member {
    int id{};
    string name;
    string department;
};

struct Transaction {
    int id{};
    int bookId{};
    int memberId{};
    string issueDate;
    string dueDate;
    string returnDate;
    int overdueDays{};
    double fine{};
    bool returned{false};
};

class Library {
private:
    vector<Book> books;
    vector<Member> members;
    vector<Transaction> transactions;
    int nextTransactionId{1};

    Book* findBook(int id) {
        for (auto& book : books)
            if (book.id == id) return &book;
        return nullptr;
    }

    const Book* findBook(int id) const {
        for (const auto& book : books)
            if (book.id == id) return &book;
        return nullptr;
    }

    const Member* findMember(int id) const {
        for (const auto& member : members)
            if (member.id == id) return &member;
        return nullptr;
    }

    bool bookIdExists(int id) const { return findBook(id) != nullptr; }

    bool memberIdExists(int id) const { return findMember(id) != nullptr; }

    int activeLoansForBook(int bookId) const {
        int count = 0;
        for (const auto& transaction : transactions)
            if (transaction.bookId == bookId && !transaction.returned) ++count;
        return count;
    }

    bool memberHasActiveLoan(int memberId, int bookId) const {
        for (const auto& transaction : transactions)
            if (transaction.memberId == memberId &&
                transaction.bookId == bookId && !transaction.returned)
                return true;
        return false;
    }

    void saveBooks() const {
        ofstream file(BOOK_FILE);
        for (const auto& b : books)
            file << b.id << '|' << b.title << '|' << b.author << '|'
                 << b.totalCopies << '\n';
    }

    void saveMembers() const {
        ofstream file(MEMBER_FILE);
        for (const auto& m : members)
            file << m.id << '|' << m.name << '|' << m.department << '\n';
    }

    void saveTransactions() const {
        ofstream file(TRANSACTION_FILE);
        for (const auto& t : transactions)
            file << t.id << '|' << t.bookId << '|' << t.memberId << '|'
                 << t.issueDate << '|' << t.dueDate << '|' << t.returnDate
                 << '|' << t.overdueDays << '|' << t.fine << '|'
                 << t.returned << '\n';
    }

    static vector<string> splitLine(const string& line) {
        vector<string> fields;
        string field;
        stringstream ss(line);
        while (getline(ss, field, '|')) fields.push_back(field);
        return fields;
    }

public:
    void loadData() {
        books.clear();
        members.clear();
        transactions.clear();
        nextTransactionId = 1;

        ifstream bookFile(BOOK_FILE);
        string line;
        while (getline(bookFile, line)) {
            auto f = splitLine(line);
            if (f.size() == 4) {
                try {
                    books.push_back({stoi(f[0]), f[1], f[2], stoi(f[3])});
                } catch (...) {}
            }
        }

        ifstream memberFile(MEMBER_FILE);
        while (getline(memberFile, line)) {
            auto f = splitLine(line);
            if (f.size() == 3) {
                try {
                    members.push_back({stoi(f[0]), f[1], f[2]});
                } catch (...) {}
            }
        }

        ifstream transactionFile(TRANSACTION_FILE);
        while (getline(transactionFile, line)) {
            auto f = splitLine(line);
            if (f.size() == 9) {
                try {
                    Transaction t;
                    t.id = stoi(f[0]);
                    t.bookId = stoi(f[1]);
                    t.memberId = stoi(f[2]);
                    t.issueDate = f[3];
                    t.dueDate = f[4];
                    t.returnDate = f[5];
                    t.overdueDays = stoi(f[6]);
                    t.fine = stod(f[7]);
                    t.returned = (stoi(f[8]) != 0);
                    transactions.push_back(t);
                    if (t.id >= nextTransactionId)
                        nextTransactionId = t.id + 1;
                } catch (...) {}
            }
        }
    }

    void addBook() {
        int id = readInt("Enter book ID: ");
        if (id <= 0 || bookIdExists(id)) {
            cout << "Invalid or duplicate book ID.\n";
            return;
        }
        string title = readText("Enter book title: ");
        string author = readText("Enter author name: ");
        int copies = readInt("Enter number of copies: ");
        if (title.empty() || author.empty() || copies <= 0 ||
            title.find('|') != string::npos || author.find('|') != string::npos) {
            cout << "Invalid details. Do not use | in text fields.\n";
            return;
        }
        books.push_back({id, title, author, copies});
        saveBooks();
        cout << GREEN << "Book added successfully!\n" << RESET;
    }

    void displayBooks() const {
        if (books.empty()) {
            cout << "No books have been added yet.\n";
            return;
        }
        cout << "\n--- BOOK CATALOG ---\n";
        cout << left << setw(8) << "ID" << setw(30) << "Title"
             << setw(24) << "Author" << setw(12) << "Total"
             << "Available\n";
        cout << string(84, '-') << '\n';
        for (const auto& b : books) {
            int available = b.totalCopies - activeLoansForBook(b.id);
            cout << left << setw(8) << b.id << setw(30) << b.title.substr(0, 28)
                 << setw(24) << b.author.substr(0, 22) << setw(12) << b.totalCopies
                 << available << '\n';
        }
    }

    void searchBook() const {
        string keyword = readText("Enter a title or author keyword: ");
        bool found = false;
        for (const auto& b : books) {
            if (b.title.find(keyword) != string::npos ||
                b.author.find(keyword) != string::npos) {
                cout << "ID: " << b.id << " | Title: " << b.title
                     << " | Author: " << b.author
                     << " | Available: "
                     << b.totalCopies - activeLoansForBook(b.id) << '\n';
                found = true;
            }
        }
        if (!found) cout << "No matching book found.\n";
    }

    void addMember() {
        int id = readInt("Enter member ID: ");
        if (id <= 0 || memberIdExists(id)) {
            cout << "Invalid or duplicate member ID.\n";
            return;
        }
        string name = readText("Enter member name: ");
        string department = readText("Enter department: ");
        if (name.empty() || department.empty() ||
            name.find('|') != string::npos ||
            department.find('|') != string::npos) {
            cout << "Invalid details. Do not use | in text fields.\n";
            return;
        }
        members.push_back({id, name, department});
        saveMembers();
        cout << GREEN << "Member added successfully!\n" << RESET;
    }

    void displayMembers() const {
        if (members.empty()) {
            cout << "No members have been added yet.\n";
            return;
        }
        cout << "\n--- MEMBER LIST ---\n";
        for (const auto& m : members)
            cout << "ID: " << m.id << " | Name: " << m.name
                 << " | Department: " << m.department << '\n';
    }

    void searchMember() const {
        int id = readInt("Enter member ID to search: ");
        const Member* member = findMember(id);
        if (!member) {
            cout << RED << "Member not found.\n" << RESET;
            return;
        }
        cout << GREEN << "\nMember found\n" << RESET
             << "ID: " << member->id << "\nName: " << member->name
             << "\nDepartment: " << member->department << '\n';
        int active = 0, past = 0;
        for (const auto& t : transactions) {
            if (t.memberId == id) {
                if (t.returned) ++past;
                else ++active;
            }
        }
        cout << "Active loans: " << active << "\nPrevious/returned loans: " << past << '\n';
    }

    void showOverdueReport() const {
        const string today = todayDate();
        int count = 0;
        cout << "\n--- OVERDUE BOOK REPORT (Today: " << today << ") ---\n";
        for (const auto& t : transactions) {
            if (t.returned) continue;
            int overdue = 0;
            if (calendarDaysBetween(t.dueDate, today, overdue) && overdue > 0) {
                const Book* b = findBook(t.bookId);
                const Member* m = findMember(t.memberId);
                cout << "Transaction ID: " << t.id
                     << " | Book: " << (b ? b->title : "Unknown book")
                     << " | Member: " << (m ? m->name : "Unknown member")
                     << " | Due: " << t.dueDate
                     << " | Overdue days: " << overdue
                     << " | Estimated fine: Tk " << fixed << setprecision(2)
                     << overdue * FINE_PER_DAY << '\n';
                ++count;
            }
        }
        if (count == 0) cout << GREEN << "No overdue books found.\n" << RESET;
        else cout << YELLOW << "Total overdue loans: " << count << RESET << '\n';
    }

    void issueBook() {
        int bookId = readInt("Enter book ID to issue: ");
        int memberId = readInt("Enter member ID: ");
        Book* book = findBook(bookId);

        if (!book) {
            cout << "Book ID not found.\n";
            return;
        }
        if (!memberIdExists(memberId)) {
            cout << "Member ID not found. Add the member first.\n";
            return;
        }
        if (book->totalCopies - activeLoansForBook(bookId) <= 0) {
            cout << "No copies are currently available.\n";
            return;
        }
        if (memberHasActiveLoan(memberId, bookId)) {
            cout << "This member already has an unreturned copy of this book.\n";
            return;
        }

        string issueDate, dueDate;
        int dateDifference = 0;
        while (true) {
            issueDate = readText("Enter issue date (DD-MM-YYYY): ");
            dueDate = readText("Enter due date (DD-MM-YYYY): ");
            if (!calendarDaysBetween(issueDate, dueDate, dateDifference)) {
                cout << RED << "Invalid date. Use DD-MM-YYYY, e.g. 22-10-2026.\n" << RESET;
                continue;
            }
            if (dateDifference < 0) {
                cout << RED << "Due date cannot be earlier than issue date. Try again.\n" << RESET;
                continue;
            }
            break;
        }

        transactions.push_back({nextTransactionId++, bookId, memberId,
                                issueDate, dueDate, "-", 0, 0.0, false});
        saveTransactions();
        cout << GREEN << "Book issued successfully!\n" << RESET;
    }

    void returnBook() {
        int bookId = readInt("Enter book ID to return: ");
        int memberId = readInt("Enter member ID: ");

        for (auto& t : transactions) {
            if (t.bookId == bookId && t.memberId == memberId && !t.returned) {
                string returnDate;
                int overdue = 0;
                while (true) {
                    returnDate = readText("Enter return date (DD-MM-YYYY): ");
                    int difference = 0;
                    int issueToReturn = 0;
                    if (!calendarDaysBetween(t.dueDate, returnDate, difference) ||
                        !calendarDaysBetween(t.issueDate, returnDate, issueToReturn)) {
                        cout << RED << "Invalid date. Use DD-MM-YYYY, e.g. 30-10-2026.\n" << RESET;
                        continue;
                    }
                    if (issueToReturn < 0) {
                        cout << RED << "Return date cannot be earlier than issue date. Try again.\n" << RESET;
                        continue;
                    }
                    // Negative means returned before due date, so there is no fine.
                    overdue = difference > 0 ? difference : 0;
                    break;
                }

                t.returnDate = returnDate;
                t.overdueDays = overdue;
                t.fine = overdue * FINE_PER_DAY;
                t.returned = true;
                saveTransactions();

                cout << GREEN << "\nBook returned successfully!\n" << RESET;
                cout << CYAN << "Overdue days: " << t.overdueDays << '\n'
                     << "Fine rate: Tk " << fixed << setprecision(2) << FINE_PER_DAY
                     << " per day\n" << RESET;
                if (t.fine > 0)
                    cout << YELLOW << "Total fine: Tk " << fixed << setprecision(2)
                         << t.fine << RESET << '\n';
                else
                    cout << GREEN << "No fine. Returned on time or early.\n" << RESET;
                return;
            }
        }
        cout << RED << "No active issue record found for that book and member.\n" << RESET;
    }

    void showTransactions() const {
        if (transactions.empty()) {
            cout << "No transaction records available.\n";
            return;
        }
        cout << "\n--- ISSUE / RETURN HISTORY ---\n";
        for (const auto& t : transactions) {
            const Book* b = findBook(t.bookId);
            const Member* m = findMember(t.memberId);
            cout << "Transaction ID: " << t.id
                 << " | Book: " << (b ? b->title : "Unknown book")
                 << " | Member: " << (m ? m->name : "Unknown member")
                 << " | Issue: " << t.issueDate
                 << " | Due: " << t.dueDate
                 << " | Return: " << t.returnDate
                 << " | Status: " << (t.returned ? "Returned" : "Issued")
                 << " | Fine: Tk " << fixed << setprecision(2) << t.fine << '\n';
        }
    }

    void showSummary() const {
        int active = 0, returned = 0;
        double totalFine = 0;
        for (const auto& t : transactions) {
            if (t.returned) {
                ++returned;
                totalFine += t.fine;
            } else {
                ++active;
            }
        }
        cout << "\n--- LIBRARY SUMMARY ---\n"
             << "Different book titles: " << books.size() << '\n'
             << "Registered members: " << members.size() << '\n'
             << "Total transactions: " << transactions.size() << '\n'
             << "Currently issued transactions: " << active << '\n'
             << "Returned transactions: " << returned << '\n'
             << fixed << setprecision(2)
             << "Total recorded fines: Tk " << totalFine << '\n';
    }
};

int main() {
    enableConsoleColors();
    Library library;
    library.loadData();

    cout << "========================================\n";
    cout << "     SMART LIBRARY MANAGEMENT SYSTEM\n";
    cout << "========================================\n";

    while (true) {
        cout << BOLD << BLUE << "\n==============================================\n"
             << "       SMART LIBRARY MANAGEMENT SYSTEM\n"
             << "==============================================\n" << RESET;
        cout << CYAN << "1. Add book\n"
             << "2. Display all books\n"
             << "3. Search book\n"
             << "4. Add member\n"
             << "5. Display all members\n"
             << "6. Issue book\n"
             << "7. Return book (automatic fine)\n"
             << "8. View transaction history\n"
             << "9. View library summary\n"
             << "10. Search member / view loan count\n"
             << "11. Overdue books report\n"
             << "0. Save and exit\n" << RESET;

        int choice = readInt("Choose an option: ");
        switch (choice) {
            case 1: library.addBook(); break;
            case 2: library.displayBooks(); break;
            case 3: library.searchBook(); break;
            case 4: library.addMember(); break;
            case 5: library.displayMembers(); break;
            case 6: library.issueBook(); break;
            case 7: library.returnBook(); break;
            case 8: library.showTransactions(); break;
            case 9: library.showSummary(); break;
            case 10: library.searchMember(); break;
            case 11: library.showOverdueReport(); break;
            case 0:
                cout << "Data is saved. Thank you for using the system.\n";
                return 0;
            default:
                cout << "Invalid choice. Please choose from the menu.\n";
        }
    }
}
