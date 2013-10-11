#include <iostream>
#include <string>
#include <vector>
#include <map>
 
using namespace std;
 
int main()
{
    typedef map<string, vector<int> > mapType;
    mapType data;
 
    // let's declare some initial values to this map
    data["BobsScore"].push_back(10);
    data["BobsScore"].push_back(11);
    data["MartysScore"].push_back(15);
    data["MartysScore"].push_back(16);
    //data["MehmetsScore"] = 34;
    //data["RockysScore"] = 22;
    //data["RockysScore"] = 23;  /*overwrites the 22 as keys are unique */
 
    // Iterate over the map and print out all key/value pairs.
    // Using a const_iterator since we are not going to change the values.

    for(mapType::const_iterator it = data.begin(); it != data.end(); ++it)
    {
        cout << "Who(key = first): " << it->first;
        cout << " Score(value1 = second): " << it->second[0];
        cout << " Score(value2 = second): " << it->second[1] << '\n';
    }
 
    return 0;
}

