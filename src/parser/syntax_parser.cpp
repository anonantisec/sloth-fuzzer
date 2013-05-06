#include <fstream>
#include <iostream> // borrame
#include "parser/syntax_parser.h"
#include "compound_field.h"
#include "exceptions.h"
#include "functions/md5.h"
#include "field.h"
#include "functions/random.h"
#include "block_field.h"
#include "compound_field.h"
#include "template_field.h"
#include "variable_block_field.h"
#include "function_value_filler.h"
#include "const_value_node.h"

std::istream *istr = nullptr;
int curr_lineno = 0;
syntax_parser* grammar_syntax_parser = nullptr;


extern "C" void yyparse();

field::filler_type default_filler() {
    return make_unique<random_function>();
}

syntax_parser::syntax_parser()
{
    register_filler_function(
        "md5", 
        [](identifier_type id) { return std::make_shared<md5_function>(id); }
    );
}

void syntax_parser::parse(const std::string &file_name)
{
    std::ifstream input(file_name);
    parse(input);
}

void syntax_parser::parse(std::istream &input)
{
    istr = &input;
    grammar_syntax_parser = this;
    
    yyparse();
    
    grammar_syntax_parser = nullptr;
    istr = nullptr;
}

void syntax_parser::add_template(std::string name, grammar::template_def_node *node)
{
    templates.insert(
        std::make_pair(std::move(name), node)
    );
}

field syntax_parser::allocate_template(const std::string &name, size_t min, size_t max)
{
    return templates.at(name)->allocate(mapper, min, max);
}

void syntax_parser::set_script(grammar::script* scr)
{
    script_root.reset(scr);
}

field syntax_parser::get_root_field()
{
    if(!script_root)
        throw parse_error();
    auto impl = make_unique<compound_field_impl>();
    
    for(const auto& i : script_root->fields)
        impl->add_field(i->allocate(mapper));
    
    return field(nullptr, std::move(impl));
}

field_mapper &syntax_parser::get_mapper()
{
    return mapper;
}

auto syntax_parser::allocate_function(const std::string &name, identifier_type id) -> filler_ptr
{
    return functions.at(name)(id);
}

// block field

auto syntax_parser::make_block_node(filler_node *filler, size_t size) -> field_node *
{
    return node_alloc<field_node>(
        [=](field_mapper &mapper) {
            return field::from_impl<block_field_impl>(
                filler ? filler->allocate(mapper) : default_filler(), 
                size
            );
        }
    );
}

auto syntax_parser::make_block_node(filler_node *filler, size_t size, 
  const std::string &name) -> field_node *
{
    auto id = mapper.find_register_field_name(name);
    return node_alloc<field_node>(
        [=](field_mapper &mapper) {
            return field::from_impl<block_field_impl>(
                id, 
                filler ? filler->allocate(mapper) : default_filler(), 
                size
            );
        }
    );
}

// variable block

auto syntax_parser::make_variable_block_node(filler_node *filler, 
  size_t min_size, size_t max_size) -> field_node *
{
    return node_alloc<field_node>(
        [=](field_mapper &mapper) {
            return field::from_impl<variable_block_field_impl>(
                filler ? filler->allocate(mapper) : default_filler(), 
                min_size,
                max_size
            );
        }
    );
}

auto syntax_parser::make_variable_block_node(filler_node *filler, 
  size_t min_size, size_t max_size, const std::string &name) 
-> field_node *
{
    auto id = mapper.find_register_field_name(name);
    return node_alloc<field_node>(
        [=](field_mapper &mapper) {
            return field::from_impl<variable_block_field_impl>(
                id, 
                filler ? filler->allocate(mapper) : default_filler(), 
                min_size,
                max_size
            );
        }
    );
}

// compound

auto syntax_parser::make_compound_field_node(fields_list *fields) -> field_node*
{
    return node_alloc<field_node>(
        [=](field_mapper &mapper) {
            auto impl = make_unique< ::compound_field_impl>();
            for(const auto &i : *fields)
                impl->add_field(i->allocate(mapper));
            return field(nullptr, std::move(impl));
        }
    );
}

// template field

auto syntax_parser::make_template_field_node(const std::string &template_name, 
  size_t min, size_t max) -> field_node*
{
    auto &parser = *this;
    return node_alloc<field_node>(
        [=, &parser](field_mapper &mapper) {
            return parser.allocate_template(template_name, min, max);
        }
    );
}

// template def field

auto syntax_parser::make_template_def_node(fields_list *fields) -> template_def_node*
{
    return node_alloc<template_def_node>(
        [&, fields](field_mapper &mapper, size_t min, size_t max) {
            auto compound_impl = make_unique< ::compound_field_impl>();
            for(const auto &i : *fields)
                compound_impl->add_field(i->allocate(mapper));
            auto impl = make_unique< ::template_field_impl>(
                            field(nullptr, std::move(compound_impl)), 
                            min, 
                            max
                        );
            return field(nullptr, std::move(impl));
        }
    );
}

auto syntax_parser::make_fields_list() -> fields_list *
{
    return node_alloc<fields_list>();
}

// stuff

auto syntax_parser::make_const_value_node(float f) -> value_node *
{
    return node_alloc<value_node>(
        [=](field_mapper &) {
            return make_unique< ::const_value_node>(f);
        }
    );
}

auto syntax_parser::make_node_value_node(const std::string &name) -> value_node *
{
    auto id = mapper.find_register_field_name(name);
    return node_alloc<value_node>(
        [=](field_mapper &) {
            return make_unique< ::node_value_function_node>(id);
        }
    );
}

auto syntax_parser::make_node_filler_node(const std::string &field_name, 
  const std::string &function_name) -> filler_node *
{
    auto id = mapper.find_register_field_name(field_name);
    auto &parser = *this;
    return node_alloc<filler_node>(
        [=, &parser](field_mapper &) {
            return allocate_function(function_name, id);
        }
    );
}

auto syntax_parser::make_function_value_filler_node(value_node *node) -> filler_node *
{
    return node_alloc<filler_node>(
        [=](field_mapper &mapper) {
            return make_unique< ::function_value_filler>(
                node->allocate(mapper)
            );
        }
    );
}

std::string *syntax_parser::make_string(const char *input)
{
    return node_alloc<std::string>(input);
}