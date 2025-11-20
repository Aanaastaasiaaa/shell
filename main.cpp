#include <iostream>
#include <string>
using namespace std;
int main() {
  string line;
  while (getline(cin, line))
  {
    if(line=="\\q")
  {
    break;
  }
  cout<<line<<endl;
  }
  return 0;
}
