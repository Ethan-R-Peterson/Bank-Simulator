// IDENTIFIER  = 292F24D17A4455C1B5133EDD8C7CEAA0C9570A98

#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <queue>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <ctime>
#include <getopt.h>

using namespace std;

// ---OPTIONS---
std::string registration_filename;
int transaction_counter = 0;
bool verbose = false;
bool query_mode = false;

void printHelp(const char* command) {
    std::cout << "Usage: " << command << " --file filename < commands.txt > output.txt\n\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help           Show this help message\n";
    std::cout << "  -f, --file filename  Specify the registration file (required)\n";
    std::cout << "  -v, --verbose        Enable verbose mode\n";
}

void getOptions(int argc, char** argv) {
    struct option long_options[] = {
        {"help", no_argument, nullptr, 'h'},
        {"file", required_argument, nullptr, 'f'},
        {"verbose", no_argument, nullptr, 'v'},
        {nullptr, 0, nullptr, 0}
    };

    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "hvf:", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                printHelp(argv[0]);
                exit(0);
            case 'v':
                verbose = true;
                break;
            case 'f':
                registration_filename = optarg;
                break;
            default:
                std::cerr << "Invalid option. Use --help to see usage.\n";
                exit(1);
        }
    }

    if (registration_filename.empty()) {
        std::cerr << "Error: registrations filename not specified.\n";
        exit(1);
    }
}

// ---OPTIONS---


// ---DATA_STRUCTURES---

struct Transaction {
    int id = 0;
    string place_timestamp;
    string exec_timestamp;
    string sender;
    string recipient;
    unsigned int amount = 0;
    char fee_type = 'o'; // 'o' or 's'
    unsigned int fee = 0;
    bool executed = false;

    Transaction() = default;
    // custom ctor for placeTransaction()
Transaction(string place_timestamp, string exec_timestamp, string sender, 
            string recipient, unsigned int amount, char fee_type) :
    place_timestamp(place_timestamp), exec_timestamp(exec_timestamp),
    sender(sender), recipient(recipient), amount(amount), fee_type(fee_type) {
        id = transaction_counter++;
    };
};

// Users
struct User {
    string pin;
    uint32_t balance;
    string reg_timestamp;
    unordered_set<string> active_ips;
    vector<Transaction> incoming;
    vector<Transaction> outgoing;
    bool logged_in = false;

    User() = default;
    // custom ctor for loading registrations.txt
    User(string pin, uint32_t balance, string reg_timestamp) : pin(pin), balance(balance), reg_timestamp(reg_timestamp) {};
};
unordered_map<string, User> users;

// ---DATA_STRUCTURES---


// ---HELPERS---

string removeColonsAndLeadingZeros(const string& timestamp) {
    string raw;
    for (char c : timestamp) {
        if (c != ':') raw += c;
    }

    // Remove leading zeros
    size_t first_nonzero = raw.find_first_not_of('0');
    if (first_nonzero != string::npos) {
        return raw.substr(first_nonzero);
    } else {
        return "0";  // all zeros
    }
}

class CompareExecDate {
public:
    bool operator()(const Transaction& a, const Transaction& b) {
        if (a.exec_timestamp != b.exec_timestamp) {
            return stoull(removeColonsAndLeadingZeros(a.exec_timestamp)) > stoull(removeColonsAndLeadingZeros(b.exec_timestamp));
        }
        return a.id > b.id;
    }
};

// Format time interval for revenue reporting





string formatTimeInterval(uint64_t start, uint64_t end) {
    string difference = to_string(end - start);

    while(difference.size() != 12) {
        difference = "0" + difference;
    }

    vector<string> parts;
    vector<string> words = {" year", " month", " day", " hour", " minute", " second" };
    // Extract two digits at a time from the end
    size_t j = 12;
    for (size_t i = 6; i > 0; i--) {
        if (difference.substr(j-2, 1) != "0") {
            parts.push_back((difference.substr(j - 2, 2)) + words[i-1]);
        } else { 
            parts.push_back((difference.substr(j - 1, 1)) + words[i-1]);
        }
        j -= 2;
    }
    string result;
    for (size_t i = parts.size(); i-- > 0;) {
        if(parts[i].substr(0,2) != "0 ") {
            if(parts[i].substr(0,2) == "1 ") {
                result += parts[i] + " ";
            } else {
                result += (parts[i] + "s ");
            }
        }
    }
    // Remove trailing space if present
    if (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }

    return result;
}




// Calculate transaction fee
unsigned int calculateTransactionFee(const Transaction& t) {
    unsigned int fee = t.amount / 100; // 1% of amount
    fee = max(10u, min(450u, fee));   // Apply min/max
    
    // Check if sender is a longstanding customer (>5 years)
    uint64_t sender_reg = stoull(removeColonsAndLeadingZeros(users[t.sender].reg_timestamp)); 
    uint64_t exec_time = stoull(removeColonsAndLeadingZeros(t.exec_timestamp));
    if (exec_time - sender_reg > 50000000000) {
        fee = (fee * 3) / 4; // 25% discount
    }
    
    return fee;
}

//---HELPERS---

// ---FORWARD_DECLARATIONS---

struct Transaction;
struct User;
// Global variables;
string current_timestamp;
string last_place_timestamp;
priority_queue<Transaction, vector<Transaction>, CompareExecDate> transaction_queue;
vector<Transaction> transaction_history;

// ---FORWARD_DECLARATIONS---


//---BIGGER_FUNCTIONS---

// LOAD REGISTRTIONS
void loadRegistrationFile(const string& filename) {
    ifstream file(filename);
    if (!file) {
        cerr << "Error: Could not open registration file " << filename << "\n";
        exit(1);
    }
    
    string line;
    while (getline(file, line)) {
        istringstream iss(line);
        string reg_timestamp, user_id, pin, balance_str;
        
        getline(iss, reg_timestamp, '|');
        getline(iss, user_id, '|');
        getline(iss, pin, '|');
        getline(iss, balance_str);
        
        uint32_t balance = static_cast<uint32_t>(std::stoul(balance_str));
        users[user_id] = User(pin, balance, reg_timestamp);
        
        // Set initial current timestamp to first registration if not set
        if (current_timestamp.empty()) {
            current_timestamp = reg_timestamp;
        }
    }
}
// LOGIN
void handleLogin(const string& user_id, const string& pin, const string& ip) {
    auto it = users.find(user_id);
    if (it == users.end()) {
        if (verbose) cout << "Login failed for " << user_id << ".\n";
        return;
    }
    
    User& user = it->second;
    if (user.pin != pin) {
        if (verbose) cout << "Login failed for " << user_id << ".\n";
        return;
    }
    
    user.active_ips.insert(ip);
    user.logged_in = true;
    if (verbose) cout << "User " << user_id << " logged in.\n";
}
// LOGOUT
void handleLogout(const string& user_id, const string& ip) {
    auto it = users.find(user_id);
    if (it == users.end()) {
        if (verbose) cout << "Logout failed for " << user_id << ".\n";
        return;
    }
    
    User& user = it->second;
    if (user.active_ips.count(ip) == 0) {
        if (verbose) cout << "Logout failed for " << user_id << ".\n";
        return;
    }
    
    user.active_ips.erase(ip);
    if(user.active_ips.empty()) user.logged_in = false;
    if (verbose) cout << "User " << user_id << " logged out.\n";
}
// BALANCE
void handleBalance(const string& user_id, const string& ip) {
    auto it = users.find(user_id);
    if (it == users.end()) {
        if (verbose) cout << "User " << user_id << " does not exist.\n";
        return;
    }

    
    User& user = it->second;
    if (verbose && user.logged_in == false) {
        cout << "User " << user_id << " is not logged in.\n";
        return;
    }


    if (verbose && user.active_ips.count(ip) == 0) {
        cout << "Fraudulent balance check detected, aborting request.\n";
        return;
    }
    
    cout << "As of " << removeColonsAndLeadingZeros(current_timestamp) << ", " << user_id 
         << " has a balance of $" << user.balance << ".\n";
}

// ---BIGGER_FUNCTIONS---

// ---TRANSACTION_FUNCTIONS---

// POCESS TRANSACTIONS
void processTransactions() {
    while (!transaction_queue.empty()) {
        const Transaction &t = transaction_queue.top();
        uint64_t exec_sec = stoull(removeColonsAndLeadingZeros(t.exec_timestamp));
        uint64_t curr_sec = stoull(removeColonsAndLeadingZeros(current_timestamp));
        
        // Stop if execution time is in the future (unless in query mode)
        if (!query_mode && exec_sec > curr_sec) break;

        // Copy the transaction before popping
        Transaction t_processed = t;
        transaction_queue.pop();

        // Check if users still exist
        if (users.count(t_processed.sender) == 0 || users.count(t_processed.recipient) == 0) {
            if (verbose) cout << "Insufficient funds to process transaction " << t_processed.id << ".\n";
            continue;
        }

        User &sender = users[t_processed.sender];
        User &recipient = users[t_processed.recipient];

        // Calculate fee and required amounts
        t_processed.fee = calculateTransactionFee(t_processed);
        uint32_t sender_total = t_processed.amount + 
                              (t_processed.fee_type == 'o' ? t_processed.fee : (t_processed.fee + 1) / 2);
        uint32_t recipient_total = (t_processed.fee_type == 's' ? t_processed.fee / 2 : 0);

        // Check sufficient funds
        if (sender.balance < sender_total || recipient.balance < recipient_total) {
            if (verbose) cout << "Insufficient funds to process transaction " << t_processed.id << ".\n";
            continue; // Discard transaction
        }

        // Execute transaction
        sender.balance -= sender_total;
        recipient.balance += t_processed.amount;
        if (t_processed.fee_type == 's') {
            recipient.balance -= recipient_total;
        }

        // Record transaction
        t_processed.executed = true;
        transaction_history.push_back(t_processed);
        sender.outgoing.push_back(t_processed);
        recipient.incoming.push_back(t_processed);

        if (verbose) {
            cout << "Transaction " << t_processed.id << " executed at "
                 << removeColonsAndLeadingZeros(t_processed.exec_timestamp) << ": $"
                 << t_processed.amount << " from " << t_processed.sender << " to "
                 << t_processed.recipient << ".\n";
        }
    }
}
// PLACE TRANSACTION
void placeTransaction(const vector<string>& args) {
    // Parse arguments
    string timestamp = args[0];
    current_timestamp = timestamp;
    string ip = args[1];
    string sender = args[2];
    string recipient = args[3];
    uint32_t amount = static_cast<uint32_t>(stoul(args[4]));
    string exec_date = args[5];
    char fee_type = args[6][0];

    uint64_t place_time = stoull(removeColonsAndLeadingZeros(timestamp));
    uint64_t exec_time = stoull(removeColonsAndLeadingZeros(exec_date));

    // Error 1: Timestamp earlier than previous place command
    if (!last_place_timestamp.empty() &&
        place_time < stoull(removeColonsAndLeadingZeros(last_place_timestamp))) {
        cerr << "Invalid decreasing timestamp in 'place' command.\n";
        exit(1);
    }

    // Error 2: Execution date before current timestamp
    if (exec_time < place_time) {
        cerr << "You cannot have an execution date before the current timestamp.\n";
        exit(1);
    }

    // 1. Check sender is different from recipient
    if (sender == recipient) {
        if (verbose) cout << "Self transactions are not allowed.\n";
        return;
    }

    // 2. Check execution date is within 3 days
    if (exec_time - place_time > 3000000) {
        if (verbose) cout << "Select a time up to three days in the future.\n";
        return;
    }

    // 3. Check sender exists
    if (users.count(sender) == 0) {
        if (verbose) cout << "Sender " << sender << " does not exist.\n";
        return;
    }

    // 4. Check recipient exists
    if (users.count(recipient) == 0) {
        if (verbose) cout << "Recipient " << recipient << " does not exist.\n";
        return;
    }

    // 5. Check registration dates are BEFORE OR EQUAL to execution time
    uint64_t sender_reg = stoull(removeColonsAndLeadingZeros(users[sender].reg_timestamp));
    uint64_t recipient_reg = stoull(removeColonsAndLeadingZeros(users[recipient].reg_timestamp));
    if (exec_time < sender_reg || exec_time < recipient_reg) {
        if (verbose) cout << "At the time of execution, sender and/or recipient have not registered.\n";
        return;
    }

    // 6. Check sender is logged in
    if (!users[sender].logged_in) {
        if (verbose) cout << "Sender " << sender << " is not logged in.\n";
        return;
    }

    // 7. Check fraudulent transaction
    if (users[sender].active_ips.count(ip) == 0) {
        if (verbose) cout << "Fraudulent transaction detected, aborting request.\n";
        return;
    }

    // All checks passed — update last_place_timestamp
    last_place_timestamp = timestamp;


    // Process transactions that are due
    processTransactions();

    // Create and queue the new transaction
    Transaction t(timestamp, exec_date, sender, recipient, amount, fee_type);
    transaction_queue.push(t);

    if (verbose) {
        cout << "Transaction " << t.id << " placed at "
             << removeColonsAndLeadingZeros(timestamp)
             << ": $" << amount << " from " << sender
             << " to " << recipient << " at "
             << removeColonsAndLeadingZeros(exec_date) << ".\n";
    }
}

// ---TRANSACTION_FUNCTIONS---

// ---QUERY_FUNCTIONS---

// LIST TRANSACTIONS
void listTransactions(const string& x, const string& y) {
    if (x == y) {
        cout << "List Transactions requires a non-empty time interval.\n";
        return;
    }
    
    uint64_t x_sec = stoull(removeColonsAndLeadingZeros(x));
    uint64_t y_sec = stoull(removeColonsAndLeadingZeros(y));
    
    if (y_sec < x_sec) {
        cout << "List Transactions requires a non-empty time interval.\n";
        return;
    }
    
    vector<Transaction> results;
    for (const auto& t : transaction_history) {
        uint64_t exec_sec = stoull(removeColonsAndLeadingZeros(t.exec_timestamp));
        if (exec_sec >= x_sec && exec_sec < y_sec) {
            results.push_back(t);
        }
    }
    
    // Sort by execution time then ID
    sort(results.begin(), results.end(), [](const Transaction& a, const Transaction& b) {
        if (a.exec_timestamp != b.exec_timestamp) {
            return stoull(removeColonsAndLeadingZeros(a.exec_timestamp)) < stoull(removeColonsAndLeadingZeros(b.exec_timestamp));
        }
        return a.id < b.id;
    });
    
    // Print results
    for (const auto& t : results) {
        cout << t.id << ": " << t.sender << " sent " << t.amount 
             << " dollar" << (t.amount != 1 ? "s" : "") << " to " << t.recipient 
             << " at " << removeColonsAndLeadingZeros(t.exec_timestamp) << ".\n";
    }
    
    cout << "There " << (results.size() == 1 ? "was " : "were ") << results.size()
         << " transaction" << (results.size() == 1 ? "" : "s") 
         << " that " << (results.size() == 1 ? "was " : "were ") << "executed between time " << removeColonsAndLeadingZeros(x) << " to " << removeColonsAndLeadingZeros(y) << ".\n";
}

// CALCULATE REVENUE
void calculateRevenue(const string& x, const string& y) {
    if (x == y) {
        cout << "Bank Revenue requires a non-empty time interval.\n";
        return;
    }
    
    uint64_t x_sec = stoull(removeColonsAndLeadingZeros(x));
    uint64_t y_sec = stoull(removeColonsAndLeadingZeros(y));
    
    if (y_sec < x_sec) {
        cout << "Bank Revenue requires a non-empty time interval.\n";
        return;
    }
    
    unsigned int total_fees = 0;
    for (const auto& t : transaction_history) {
        uint64_t place_sec = stoull(removeColonsAndLeadingZeros(t.place_timestamp));
        if (place_sec >= x_sec && place_sec < y_sec) {
            total_fees += t.fee;
        }
    }
    
    uint64_t xnum = stoull(removeColonsAndLeadingZeros(x));
    uint64_t ynum = stoull(removeColonsAndLeadingZeros(y));
    // uint64_t interval = y_sec - x_sec;
    cout << "281Bank has collected " << total_fees 
         << " dollars in fees over " << formatTimeInterval(xnum, ynum) << ".\n";
}

// CUSTOMER HISTORY
void customerHistory(const string& user_id) {
    auto it = users.find(user_id);
    if (it == users.end()) {
        cout << "User " << user_id << " does not exist.\n";
        return;
    }
    
    const User& user = it->second;
    cout << "Customer " << user_id << " account summary:\n";
    cout << "Balance: $" << user.balance << "\n";
    
    size_t total_trans = user.incoming.size() + user.outgoing.size();
    cout << "Total # of transactions: " << total_trans << "\n";
    
    // Incoming transactions
    cout << "Incoming " << user.incoming.size() << ":" << "\n";
    size_t start_in = (user.incoming.size() > 10) ? user.incoming.size() - 10 : 0;
    for (size_t i = start_in; i < user.incoming.size(); ++i) {
        const auto& t = user.incoming[i];
        cout << t.id << ": " << t.sender << " sent " << t.amount 
             << " dollar" << (t.amount != 1 ? "s" : "") << " to " << t.recipient 
             << " at " << removeColonsAndLeadingZeros(t.exec_timestamp) << ".\n";
    }
    
    // Outgoing transactions
    cout << "Outgoing " << user.outgoing.size() << ":" << "\n";
    size_t start_out = (user.outgoing.size() > 10) ? user.outgoing.size() - 10 : 0;
    for (size_t i = start_out; i < user.outgoing.size(); ++i) {
        const auto& t = user.outgoing[i];
        cout << t.id << ": " << t.sender << " sent " << t.amount 
             << " dollar" << (t.amount != 1 ? "s" : "") << " to " << t.recipient 
             << " at " << removeColonsAndLeadingZeros(t.exec_timestamp) << ".\n";
    }
}

void summarizeDay(const string& timestamp) {
    // Extract day part (yy:mm:dd)
    string day_part = timestamp.substr(0, 8); // "yy:mm:dd"
    string next_day = day_part;
    
    // Increment day (properly handling all components)
    int year = stoi(next_day.substr(0, 2));
    int month = stoi(next_day.substr(3, 2));
    int day = stoi(next_day.substr(6, 2));
    
    day++;
    // Handle component overflow (all components can be 00-99)
    if (day > 99) {
        day = 0;
        month++;
        if (month > 99) {
            month = 0;
            year++;
            if (year > 99) {
                year = 0;
            }
        }
    }
    
    // Format next_day with leading zeros
    next_day = 
        (year < 10 ? "0" : "") + to_string(year) + ":" +
        (month < 10 ? "0" : "") + to_string(month) + ":" +
        (day < 10 ? "0" : "") + to_string(day);
    
    string start_time = day_part + "00:00:00";
    string end_time = next_day + "00:00:00";
    
    vector<Transaction> results;
    unsigned int total_fees = 0;
    
    for (const auto& t : transaction_history) {
        if (t.exec_timestamp >= start_time && t.exec_timestamp < end_time) {
            results.push_back(t);
            total_fees += t.fee;
        }
    }
    
    // Format timestamps for output - just remove colons from the full timestamps
    string formatted_start = removeColonsAndLeadingZeros(start_time);
    string formatted_end = removeColonsAndLeadingZeros(end_time);
    
    cout << "Summary of [" << formatted_start << ", " << formatted_end << "):\n";
    
    // Sort by execution time then ID
    sort(results.begin(), results.end(), [](const Transaction& a, const Transaction& b) {
        if (a.exec_timestamp != b.exec_timestamp) {
            return stoull(removeColonsAndLeadingZeros(a.exec_timestamp)) < stoull(removeColonsAndLeadingZeros(b.exec_timestamp));
        }
        return a.id < b.id;
    });
    
    for (const auto& t : results) {
        cout << t.id << ": " << t.sender << " sent " << t.amount 
             << " dollar" << (t.amount != 1 ? "s" : "") << " to " << t.recipient 
             << " at " << removeColonsAndLeadingZeros(t.exec_timestamp) << ".\n";
    }
    
    cout << "There " << (results.size() == 1 ? "was " : "were ") << "a total of " << results.size()
         << " transaction" << (results.size() == 1 ? "" : "s") << ", "
         << "281Bank has collected " << total_fees << " dollars in fees.\n";
}
// ---QUERY_FUNCTIONS---


// ------MAIN------
int main(int argc, char* argv[]) {

    getOptions(argc, argv);
    // *received registration file in getOptions
    loadRegistrationFile(registration_filename);
    
    string line;
    
    while (getline(cin, line)) {
        if (line.empty()) continue;
        
        if (line == "$$$") {
            query_mode = true;
            // Process any remaining transactions
            while (!transaction_queue.empty()) {
                processTransactions();
            }
            continue;
        }
        
        if (line[0] == '#') continue; // Skip comments
        
        istringstream iss(line);
        string command;
        iss >> command;
        
        if (!query_mode) {
            // Operation commands
            if (command == "login") {
                string user_id, pin, ip;
                iss >> user_id >> pin >> ip;
                handleLogin(user_id, pin, ip);
            }
            else if (command == "out") {
                string user_id, ip;
                iss >> user_id >> ip;
                handleLogout(user_id, ip);
            }
            else if (command == "balance") {
                string user_id, ip;
                iss >> user_id >> ip;
                handleBalance(user_id, ip);
            }
            else if (command == "place") {
                vector<string> args;
                string arg;
                while (iss >> arg) {
                    args.push_back(arg);
                }
                if (args.size() != 7) {
                    cerr << "Invalid place command\n";
                    continue;
                }
                placeTransaction(args);
            }
        }
        else {
            // Query commands
            if (command == "l") {
                string x, y;
                iss >> x >> y;
                listTransactions(x, y);
            }
            else if (command == "r") {
                string x, y;
                iss >> x >> y;
                calculateRevenue(x, y);
            }
            else if (command == "h") {
                string user_id;
                iss >> user_id;
                customerHistory(user_id);
            }
            else if (command == "s") {
                string timestamp;
                iss >> timestamp;
                summarizeDay(timestamp);
            }
            else if (command == "") {
                break;
            }
        }
    }
    
    return 0;
}