#include <iostream>
#include <string>
#include <vector>
#include <fstream>

using namespace std;

int main() 
{
    vector<string> history;
    string input;
    bool running = true;
<<<<<<< HEAD
    string history_file = "kubsh_history.txt";
    ofstream write_file(history_file, ios::app);
=======
    
>>>>>>> 804df1a20050d6c37a77f085e30ceb54cdb028b1
    while (running && getline(cin, input)) 
    {
        if (input.empty()) {
            continue;
        }
<<<<<<< HEAD
        write_file << input << endl;
        write_file.flush();
=======
        
>>>>>>> 804df1a20050d6c37a77f085e30ceb54cdb028b1
        if (input == "\\q") 
        {
            running = false;
            break;
        }
        else if (input.find("debug ") == 0) 
        {
            string text = input.substr(6);
<<<<<<< HEAD
            
            if (text.size() >= 2) 
=======
            if (text.size() >= 2)
>>>>>>> 804df1a20050d6c37a77f085e30ceb54cdb028b1
            {
                char first = text[0];
                char last = text[text.size()-1];
                if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) 
                {
                    text = text.substr(1, text.size()-2);
                }
            }
<<<<<<< HEAD
            
            cout << text << endl;
            history.push_back(input);
        }
        else 
        {
            
            cout << input << ": command not found" << endl;
            history.push_back(input);
        }
     write_file.close();

    }
    
=======
            cout << text << endl;
            history.push_back(input);
        }
        else
        {
            cout << input << ": command not found" << endl;
            history.push_back(input);
        }
    }
>>>>>>> 804df1a20050d6c37a77f085e30ceb54cdb028b1
    return 0;
}
