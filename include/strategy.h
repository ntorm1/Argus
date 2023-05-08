
#include <functional>
#include <string>

using namespace std;

class Strategy
{
public:
    //get the unique id of the strategy
    const string & get_strategy_id(){return this->strategy_id;}

    std::function<void()> python_handler_on_open;
    std::function<void()> cxx_handler_on_open;

    std::function<void()> python_handler_on_close;
    std::function<void()> cxx_handler_on_close;

    Strategy(string strategy_id_)
    {
        // assign strategy id
        this->strategy_id = strategy_id_;

        // set c++ lambda functions to point to return the python functions registered
        cxx_handler_on_open = [this](void) 
        { 
            // call python object's on_close() method to generate orders
            return python_handler_on_open(); 
        };
        cxx_handler_on_close = [this](void) 
        {
            // call python object's on_close() method to generate orders
            return python_handler_on_close(); 

        };
    }

private:
    ///unique id of the strategy
    string strategy_id;

};