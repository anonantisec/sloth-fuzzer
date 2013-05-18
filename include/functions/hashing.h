#ifndef FUZZER_FUNCTION_HASHING_H
#define FUZZER_FUNCTION_HASHING_H

#include <vector>
#include "constraints.h"
#include "unary_field_function.h"

class md5_function : public unary_field_filler_function<md5_function> {
public:
    md5_function(identifier_type id);
    
    void apply(const field &input_field, field &output_field);
    
    static const std::array<constraint_type, 1> &get_constraints();
};

class sha1_function : public unary_field_filler_function<sha1_function> {
public:
    sha1_function(identifier_type id);
    
    void apply(const field &input_field, field &output_field);
    
    static const std::array<constraint_type, 1> &get_constraints();
};

#endif // FUZZER_FUNCTION_HASHING_H