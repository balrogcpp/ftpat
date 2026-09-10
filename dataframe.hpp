// DataFrame.hpp
#pragma once


#include "any.hpp"
#include "fast_float.h"
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <stdexcept>
#include <memory>
#include <typeinfo>
#include <typeindex>
#include <functional>
#include <numeric>
#include <span>


// Base class for column storage
class ColumnBase {
public:
    virtual ~ColumnBase() = default;
    virtual size_t size() const = 0;
    virtual std::type_index type() const = 0;
    virtual std::unique_ptr<ColumnBase> clone() const = 0;
    virtual std::string to_string(size_t index) const = 0;
    virtual bool compare(size_t index, const std::string& op, const linb::any& value) const = 0;
    virtual linb::any get_any(size_t index) const = 0;
    virtual void set_any(size_t index, const linb::any& value) = 0;
    virtual void push_back_any(const linb::any& value) = 0;
    virtual void reserve(size_t size) = 0;
    virtual void resize(size_t size) = 0;
};


// Template class for column storage
template<typename T>
class Column : public ColumnBase {
private:
    std::vector<T> data;
    
public:
    Column() = default;
    explicit Column(const std::vector<T>& vec) : data(vec) {}
    explicit Column(std::vector<T>&& vec) : data(std::move(vec)) {}
    explicit Column(size_t size) : data(size) {}
    explicit Column(size_t size, const T& value) : data(size, value) {}
    
    static std::string any_print(const linb::any &value) {
        if (auto x = linb::any_cast<int>(&value)) {
            return std::to_string(*x);;
        }
        else if (auto x = linb::any_cast<float>(&value)) {
            return std::to_string(*x);
        }
        else if (auto x = linb::any_cast<double>(&value)) {
            return std::to_string(*x);
        }
        else if (auto x = linb::any_cast<std::string>(&value)) {
            return *x;
        }

        return "N/A";
    }

    size_t size() const override { return data.size(); }
    std::type_index type() const override { return std::type_index(typeid(T)); }
    
    std::unique_ptr<ColumnBase> clone() const override {
        return std::make_unique<Column<T>>(data);
    }
    
    std::string to_string(size_t index) const override {
        if (index >= data.size()) {
            throw std::out_of_range("Index out of range");
        }
        // if constexpr (std::is_same_v<T, std::string>) {
        //     return data[index];
        // } else {
        //     return std::to_string(data[index]);
        // }
        
        return any_print(data[index]);
    }
    
    linb::any get_any(size_t index) const override {
        if (index >= data.size()) throw std::out_of_range("Index out of range");
        return linb::any(data[index]);
    }
    
    void set_any(size_t index, const linb::any& value) override {
        if (index >= data.size()) throw std::out_of_range("Index out of range");
        try {
            data[index] = linb::any_cast<T>(value);
        } catch (const linb::bad_any_cast&) {
            throw std::runtime_error("Type mismatch in set_any");
        }
    }
    
    void push_back_any(const linb::any& value) override {
        try {
            data.push_back(linb::any_cast<T>(value));
        } catch (const linb::bad_any_cast&) {
            throw std::runtime_error("Type mismatch in push_back_any");
        }
    }

    bool compare(size_t index, const std::string& op, const linb::any& value) const override {
        if (index >= data.size()) return false;
        try {
            T val = linb::any_cast<T>(value);
            if (op == "==") return data[index] == val;
            else if (op == "!=") return data[index] != val;
            else if (op == "<") return data[index] < val;
            else if (op == "<=") return data[index] <= val;
            else if (op == ">") return data[index] > val;
            else if (op == ">=") return data[index] >= val;
            else throw std::invalid_argument("Invalid operator: " + op);
        } catch (const linb::bad_any_cast&) {
            throw std::runtime_error("Type mismatch in comparison");
        }
    }
    
    // Get value at index
    T get(size_t index) const { 
        if (index >= data.size()) throw std::out_of_range("Index out of range");
        return data[index]; 
    }
    
    // Set value at index
    void set(size_t index, const T& value) {
        if (index >= data.size()) throw std::out_of_range("Index out of range");
        data[index] = value;
    }
    
    // Push back a value
    void push_back(const T& value) { data.push_back(value); }
    void push_back(T&& value) { data.push_back(std::move(value)); }
    
    // Reserve space
    void reserve(size_t size) { data.reserve(size); }
    
    // Resize
    void resize(size_t size) { data.resize(size); }
    void resize(size_t size, const T& value) { data.resize(size, value); }

    // Get reference to underlying vector
    const std::vector<T>& get_data() const { return data; }
    std::vector<T>& get_data() { return data; }
};

// DataFrame class
class DataFrame {
private:
    std::unordered_map<std::string, std::unique_ptr<ColumnBase>> columns;
    std::vector<std::string> column_names;
    std::vector<std::string> row_names;
    std::unordered_map<std::string, size_t> row_name_to_index;
    size_t row_count = 0;
    
public:
    // Constructors
    DataFrame() = default;
    
    // Constructor from initializer list
    DataFrame(const std::unordered_map<std::string, std::vector<linb::any>>& data) {
        for (const auto& [name, vec] : data) {
            if (vec.empty()) continue;
            add_column(name, vec);
        }
        update_row_count();
    }
    
    // Copy constructor
    DataFrame(const DataFrame& other) {
        for (const auto& [name, col] : other.columns) {
            column_names.push_back(name);
            columns[name] = col->clone();
        }
        row_count = other.row_count;
    }
    
    // Move constructor
    DataFrame(DataFrame&& other) noexcept 
        : columns(std::move(other.columns)), 
          column_names(std::move(other.column_names)),
          row_count(other.row_count) {
        other.row_count = 0;
    }
    
    // Assignment operators
    DataFrame& operator=(const DataFrame& other) {
        if (this != &other) {
            columns.clear();
            column_names.clear();
            for (const auto& [name, col] : other.columns) {
                column_names.push_back(name);
                columns[name] = col->clone();
            }
            row_count = other.row_count;
        }
        return *this;
    }
    
    DataFrame& operator=(DataFrame&& other) noexcept {
        if (this != &other) {
            columns = std::move(other.columns);
            column_names = std::move(other.column_names);
            row_count = other.row_count;
            other.row_count = 0;
        }
        return *this;
    }

    // Add column with move semantics
    template<typename T>
    void add_column(const std::string& name, std::vector<T>&& data) {
        if (columns.find(name) != columns.end()) {
            throw std::runtime_error("Column already exists: " + name);
        }
        if (!column_names.empty() && data.size() != row_count) {
            throw std::runtime_error("Data size mismatch: expected " + 
                                   std::to_string(row_count) + ", got " + 
                                   std::to_string(data.size()));
        }
        columns[name] = std::make_unique<Column<T>>(std::move(data));
        column_names.push_back(name);
        update_row_count();
    }
    
    // Add column with default values
    template<typename T>
    void add_column(const std::string& name, size_t size, const T& default_value = T()) {
        if (columns.find(name) != columns.end()) {
            throw std::runtime_error("Column already exists: " + name);
        }
        if (!column_names.empty() && size != row_count) {
            throw std::runtime_error("Size mismatch: expected " + 
                                   std::to_string(row_count) + ", got " + 
                                   std::to_string(size));
        }
        columns[name] = std::make_unique<Column<T>>(size, default_value);
        column_names.push_back(name);
        update_row_count();
    }

    // Add a column
    template<typename T>
    void add_column(const std::string& name, const std::vector<T>& data) {
        if (columns.find(name) != columns.end()) {
            throw std::runtime_error("Column already exists: " + name);
        }
        if (!column_names.empty() && data.size() != row_count) {
            throw std::runtime_error("Data size mismatch: expected " + std::to_string(row_count) + 
                                   ", got " + std::to_string(data.size()));
        }
        columns[name] = std::make_unique<Column<T>>(data);
        column_names.push_back(name);
        update_row_count();
    }
    
    // Add column from vector of any (for flexibility)
    void add_column(const std::string& name, const std::vector<linb::any>& data) {
        if (data.empty()) throw std::runtime_error("Cannot add empty column");
        
        // Determine type from first element
        const auto& first = data[0];
        if (first.type() == typeid(int)) {
            std::vector<int> typed_data;
            typed_data.reserve(data.size());
            for (const auto& val : data) {
                typed_data.push_back(linb::any_cast<int>(val));
            }
            add_column(name, typed_data);
        } else if (first.type() == typeid(double)) {
            std::vector<double> typed_data;
            typed_data.reserve(data.size());
            for (const auto& val : data) {
                typed_data.push_back(linb::any_cast<double>(val));
            }
            add_column(name, typed_data);
        } else if (first.type() == typeid(std::string)) {
            std::vector<std::string> typed_data;
            typed_data.reserve(data.size());
            for (const auto& val : data) {
                typed_data.push_back(linb::any_cast<std::string>(val));
            }
            add_column(name, typed_data);
        } else {
            throw std::runtime_error("Unsupported type for column");
        }
    }
    
    // Get column as vector
    template<typename T>
    std::vector<T> get_column(const std::string& name) const {
        auto it = columns.find(name);
        if (it == columns.end()) {
            throw std::runtime_error("Column not found: " + name);
        }
        auto* col = dynamic_cast<Column<T>*>(it->second.get());
        if (!col) {
            throw std::runtime_error("Type mismatch for column: " + name);
        }
        return col->get_data();
    }
    
    template<typename T>
    std::vector<T> column(const std::string& name) const {
        auto it = columns.find(name);
        if (it == columns.end()) {
            throw std::runtime_error("Column not found: " + name);
        }
        auto* col = dynamic_cast<Column<T>*>(it->second.get());
        if (!col) {
            throw std::runtime_error("Type mismatch for column: " + name);
        }
        return col->get_data();
    }

    // Get value at row and column
    template<typename T>
    T get_value(size_t row, const std::string& column_name) const {
        auto it = columns.find(column_name);
        if (it == columns.end()) {
            throw std::runtime_error("Column not found: " + column_name);
        }
        auto* col = dynamic_cast<Column<T>*>(it->second.get());
        if (!col) {
            throw std::runtime_error("Type mismatch for column: " + column_name);
        }
        return col->get(row);
    }
    
    // Set value at row and column
    template<typename T>
    void set_value(size_t row, const std::string& column_name, const T& value) {
        auto it = columns.find(column_name);
        if (it == columns.end()) {
            throw std::runtime_error("Column not found: " + column_name);
        }
        auto* col = dynamic_cast<Column<T>*>(it->second.get());
        if (!col) {
            throw std::runtime_error("Type mismatch for column: " + column_name);
        }
        col->set(row, value);
    }
    
    // Add row
    DataFrame& add_row(const std::unordered_map<std::string, linb::any>& row_data) {
        for (const auto& [name, value] : row_data) {
            auto it = columns.find(name);
            if (it == columns.end()) {
                throw std::runtime_error("Column not found: " + name);
            }
            
            // Use type information to push back
            const auto& type = value.type();
            if (type == typeid(int)) {
                auto* col = dynamic_cast<Column<int>*>(it->second.get());
                if (col) col->push_back(linb::any_cast<int>(value));
            } else if (type == typeid(double)) {
                auto* col = dynamic_cast<Column<double>*>(it->second.get());
                if (col) col->push_back(linb::any_cast<double>(value));
            } else if (type == typeid(std::string)) {
                auto* col = dynamic_cast<Column<std::string>*>(it->second.get());
                if (col) col->push_back(linb::any_cast<std::string>(value));
            } else {
                throw std::runtime_error("Unsupported type for row value");
            }
        }
        row_count++;
        return *this;
    }

    // Remove row
    void remove_row(size_t row) {
        if (row >= row_count) {
            throw std::out_of_range("Row index out of range");
        }
        
        // Shift rows up
        for (size_t i = row; i < row_count - 1; ++i) {
            for (const auto& name : column_names) {
                auto it = columns.find(name);
                if (it != columns.end()) {
                    auto val = it->second->get_any(i + 1);
                    it->second->set_any(i, val);
                }
            }
        }
        
        // Resize all columns
        for (auto& [name, col] : columns) {
            col->resize(row_count - 1);
        }
        row_count--;
    }
    
    // Remove multiple rows
    void remove_rows(const std::vector<size_t>& indices) {
        std::vector<size_t> sorted_indices = indices;
        std::sort(sorted_indices.begin(), sorted_indices.end(), std::greater<size_t>());
        
        for (size_t idx : sorted_indices) {
            if (idx < row_count) {
                remove_row(idx);
            }
        }
    }

    // Filter rows based on condition
    DataFrame filter(const std::string& column_name, 
                    const std::string& op, 
                    const linb::any& value) const {
        auto it = columns.find(column_name);
        if (it == columns.end()) {
            throw std::runtime_error("Column not found: " + column_name);
        }
        
        std::vector<bool> mask;
        mask.reserve(row_count);
        for (size_t i = 0; i < row_count; ++i) {
            mask.push_back(it->second->compare(i, op, value));
        }
        
        return filter_by_mask(mask);
    }
    
    // Filter with custom function
    template<typename Func>
    DataFrame filter_custom(const std::string& column_name, Func func) const {
        auto it = columns.find(column_name);
        if (it == columns.end()) {
            throw std::runtime_error("Column not found: " + column_name);
        }
        
        std::vector<bool> mask;
        mask.reserve(row_count);
        for (size_t i = 0; i < row_count; ++i) {
            // This is a simplified version - would need proper type handling
            mask.push_back(func(i));
        }
        
        return filter_by_mask(mask);
    }
    
    // Filter by boolean mask
    DataFrame filter_by_mask(const std::vector<bool>& mask) const {
        if (mask.size() != row_count) {
            throw std::runtime_error("Mask size mismatch");
        }
        
        DataFrame result;
        
        for (const auto& name : column_names) {
            auto it = columns.find(name);
            if (it == columns.end()) continue;
            
            const auto& col = it->second;
            std::vector<linb::any> filtered_data;
            
            for (size_t i = 0; i < row_count; ++i) {
                if (mask[i]) {
                    const auto& type = col->type();
                    if (type == typeid(int)) {
                        filtered_data.push_back(dynamic_cast<Column<int>*>(col.get())->get(i));
                    } else if (type == typeid(double)) {
                        filtered_data.push_back(dynamic_cast<Column<double>*>(col.get())->get(i));
                    } else if (type == typeid(std::string)) {
                        filtered_data.push_back(dynamic_cast<Column<std::string>*>(col.get())->get(i));
                    }
                }
            }
            
            if (!filtered_data.empty()) {
                result.add_column(name, filtered_data);
            }
        }
        
        return result;
    }
    
    // Group by a column and aggregate
    template<typename T, typename AggFunc>
    DataFrame group_by(const std::string& group_column, 
                      const std::string& agg_column, 
                      AggFunc agg_func) const {
        auto group_it = columns.find(group_column);
        auto agg_it = columns.find(agg_column);
        
        if (group_it == columns.end() || agg_it == columns.end()) {
            throw std::runtime_error("Column not found");
        }
        
        auto* group_col = dynamic_cast<Column<T>*>(group_it->second.get());
        if (!group_col) {
            throw std::runtime_error("Type mismatch for group column");
        }
        
        // Group data
        std::unordered_map<T, std::vector<double>> groups;
        for (size_t i = 0; i < row_count; ++i) {
            T key = group_col->get(i);
            auto* agg_col = dynamic_cast<Column<double>*>(agg_it->second.get());
            if (agg_col) {
                groups[key].push_back(agg_col->get(i));
            }
        }
        
        // Create result DataFrame
        DataFrame result;
        std::vector<T> keys;
        std::vector<double> aggregated;
        
        for (const auto& [key, values] : groups) {
            keys.push_back(key);
            aggregated.push_back(agg_func(values));
        }
        
        result.add_column(group_column, keys);
        result.add_column("aggregated_" + agg_column, aggregated);
        
        return result;
    }
    
    // Sort by column
    template<typename T>
    DataFrame sort_by(const std::string& column_name, bool ascending = true) const {
        auto it = columns.find(column_name);
        if (it == columns.end()) {
            throw std::runtime_error("Column not found: " + column_name);
        }
        
        auto* col = dynamic_cast<Column<T>*>(it->second.get());
        if (!col) {
            throw std::runtime_error("Type mismatch for column: " + column_name);
        }
        
        std::vector<size_t> indices(row_count);
        std::iota(indices.begin(), indices.end(), 0);
        
        const auto& data = col->get_data();
        if (ascending) {
            std::sort(indices.begin(), indices.end(), 
                [&data](size_t i, size_t j) { return data[i] < data[j]; });
        } else {
            std::sort(indices.begin(), indices.end(), 
                [&data](size_t i, size_t j) { return data[i] > data[j]; });
        }
        
        return select_rows(indices);
    }
    
    // Select specific rows
    DataFrame select_rows(const std::vector<size_t>& indices) const {
        DataFrame result;
        
        for (const auto& name : column_names) {
            auto it = columns.find(name);
            if (it == columns.end()) continue;
            
            const auto& col = it->second;
            std::vector<linb::any> selected_data;
            
            for (size_t idx : indices) {
                const auto& type = col->type();
                if (type == typeid(int)) {
                    selected_data.push_back(dynamic_cast<Column<int>*>(col.get())->get(idx));
                } else if (type == typeid(double)) {
                    selected_data.push_back(dynamic_cast<Column<double>*>(col.get())->get(idx));
                } else if (type == typeid(std::string)) {
                    selected_data.push_back(dynamic_cast<Column<std::string>*>(col.get())->get(idx));
                }
            }
            
            if (!selected_data.empty()) {
                result.add_column(name, selected_data);
            }
        }
        
        return result;
    }
    
    // Select specific columns
    DataFrame select_columns(const std::vector<std::string>& names) const {
        DataFrame result;
        for (const auto& name : names) {
            auto it = columns.find(name);
            if (it == columns.end()) {
                throw std::runtime_error("Column not found: " + name);
            }
            result.columns[name] = it->second->clone();
            result.column_names.push_back(name);
        }
        result.update_row_count();
        return result;
    }
    
    // Drop column
    DataFrame drop_column(const std::string& name) const {
        DataFrame result = *this;
        result.columns.erase(name);
        auto it = std::find(result.column_names.begin(), result.column_names.end(), name);
        if (it != result.column_names.end()) {
            result.column_names.erase(it);
        }
        result.update_row_count();
        return result;
    }
    
    // Rename column
    DataFrame rename_column(const std::string& old_name, const std::string& new_name) const {
        DataFrame result = *this;
        auto it = result.columns.find(old_name);
        if (it == result.columns.end()) {
            throw std::runtime_error("Column not found: " + old_name);
        }
        
        auto col = std::move(it->second);
        result.columns.erase(old_name);
        result.columns[new_name] = std::move(col);
        
        auto name_it = std::find(result.column_names.begin(), result.column_names.end(), old_name);
        if (name_it != result.column_names.end()) {
            *name_it = new_name;
        }
        
        return result;
    }

    // Head method - first n rows
    DataFrame head(size_t n = 5) const {
        size_t count = std::min(n, row_count);
        std::vector<size_t> indices(count);
        std::iota(indices.begin(), indices.end(), 0);
        return select_rows(indices);
    }
    
    // Tail method - last n rows
    DataFrame tail(size_t n = 5) const {
        size_t count = std::min(n, row_count);
        std::vector<size_t> indices(count);
        for (size_t i = 0; i < count; ++i) {
            indices[i] = row_count - count + i;
        }
        return select_rows(indices);
    }
    
    // Shape
    std::pair<size_t, size_t> shape() const {
        return {row_count, column_names.size()};
    }
    
    // Get column names
    std::vector<std::string> get_column_names() const {
        return column_names;
    }
    
    // Get row count
    size_t get_row_count() const { return row_count; }
    
    // Info - print basic information
    void info() const {
        std::cout << "DataFrame Info:\n";
        std::cout << "  Shape: (" << row_count << ", " << column_names.size() << ")\n";
        std::cout << "  Columns:\n";
        for (const auto& name : column_names) {
            auto it = columns.find(name);
            if (it != columns.end()) {
                std::cout << "    " << name << ": " << it->second->type().name() 
                         << " (" << it->second->size() << " rows)\n";
            }
        }
    }
    
    // Print DataFrame
    void print(size_t max_rows = 10) const {
        size_t rows_to_show = std::min(max_rows, row_count);
        if (rows_to_show == 0) {
            std::cout << "Empty DataFrame\n";
            return;
        }
        
        // Print headers
        for (const auto& name : column_names) {
            std::cout << name << "\t";
        }
        std::cout << "\n";
        
        // Print data
        for (size_t i = 0; i < rows_to_show; ++i) {
            for (const auto& name : column_names) {
                auto it = columns.find(name);
                if (it != columns.end()) {
                    std::cout << it->second->to_string(i) << "\t";
                }
            }
            std::cout << "\n";
        }
        
        if (row_count > max_rows) {
            std::cout << "... (" << (row_count - max_rows) << " more rows)\n";
        }
    }
    
private:
    void update_row_count() {
        if (!columns.empty()) {
            row_count = columns.begin()->second->size();
        }
    }
};
