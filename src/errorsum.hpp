/***
 * ErrorSum manages the error types of a node. This is necessary, because
 * the status of a node changes over the runtime.
 */

#ifndef ERRORSUM_HPP_
#define ERRORSUM_HPP_

using namespace std;

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))

class ErrorSum {

    short error_sum;

public:

    ErrorSum() :
    error_sum(0){
    }

    void set_to_normal() {
        error_sum = 0;
    }

    void set_direction_error() {
        if (!is_direction_error()) error_sum += 1;
    }

    void set_name_error() {
        if (!is_name_error()) error_sum += 2;
    }

    void set_type_error() {
        if (!is_type_error()) error_sum += 4;
    }

    void set_spring_error() {
        if (!is_spring_error()) error_sum += 8;
    }

    void set_end_error() {
        if (!is_end_error()) error_sum += 16;
    }

    void set_rivermouth() {
        error_sum = 32;
    }

    void set_outflow() {
        error_sum = 64;
    }

    void set_poss_rivermouth() {
        error_sum = 128;
    }

    void set_poss_outflow() {
        error_sum = 256;
    }

    void set_stream() {
        if (!is_stream()) error_sum  += 512;
    }

    void set_river() {
        if (!is_river()) error_sum += 1024;
    }

    void set_way_error() {
        error_sum += 2048;
    }

    bool is_normal() {
        return (!error_sum);
    }

    bool is_direction_error() {
        return (CHECK_BIT(error_sum,0));
    }

    bool is_name_error() {
        return (CHECK_BIT(error_sum,1));
    }

    bool is_type_error() {
        return (CHECK_BIT(error_sum,2));
    }

    bool is_spring_error() {
        return (CHECK_BIT(error_sum,3));
    }

    bool is_end_error() {
        return (CHECK_BIT(error_sum,4));
    }

    bool is_rivermouth() {
        return (CHECK_BIT(error_sum,5));
    }

    bool is_outflow() {
        return (CHECK_BIT(error_sum,6));
    }

    bool is_poss_rivermouth() {
        return (CHECK_BIT(error_sum,7));
    }

    bool is_poss_outflow() {
        return (CHECK_BIT(error_sum,8));
    }

    bool is_stream() {
        return (CHECK_BIT(error_sum,9));
    }

    bool is_river() {
        return (CHECK_BIT(error_sum,10));
    }

    bool is_way_error() {
        return (CHECK_BIT(error_sum,11));
    }

    //DEBUG
    short errsum() {
        return error_sum;
    }
    /***
     * switch the possible mouths to dedicated spring errors and end errors.
     */
    void switch_poss() {
        if (is_river()) {
            if (is_poss_rivermouth()) {
                error_sum -= 128;
                set_end_error();
            } else if (is_poss_outflow()) {
                error_sum -= 256;
                set_spring_error();
            }
        }
    }
};

#endif /* ERRORSUM_HPP_ */
