#pragma once
#ifndef FTPAT_DATASET_HPP
#define FTPAT_DATASET_HPP

#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

template <typename T>
class Dataset {
public:
	Dataset() : row_count_(0) {}

	std::size_t RowCount() const { return row_count_; }
	std::size_t ColumnCount() const { return columns_.size(); }

	void AddColumn(const std::vector<T>& values) {
		AddColumn(std::string(), values);
	}

	void AddColumn(const std::string& name, const std::vector<T>& values) {
		if (columns_.empty()) {
			if (row_count_ != 0 && values.size() != row_count_)
				throw std::invalid_argument("Column size does not match row count");
			if (row_count_ == 0)
				row_count_ = values.size();
		} else if (values.size() != row_count_) {
			throw std::invalid_argument("Column size does not match row count");
		}

		CheckNewName(name, column_indices_);
		columns_.push_back(values);
		if (!name.empty())
			column_indices_[name] = columns_.size() - 1;
	}

	void AddRow(const std::vector<T>& values) {
		AddRow(std::string(), values);
	}

	void AddRow(const std::string& name, const std::vector<T>& values) {
		if (values.size() != columns_.size())
			throw std::invalid_argument("Row size does not match column count");
		CheckNewName(name, row_indices_);
		for (std::size_t column = 0; column < columns_.size(); ++column)
			columns_[column].push_back(values[column]);
		++row_count_;
		if (!name.empty())
			row_indices_[name] = row_count_ - 1;
	}

	void RemoveRow(std::size_t row) {
		if (row >= row_count_)
			throw std::out_of_range("Dataset row index out of range");

		for (std::size_t column = 0; column < columns_.size(); ++column)
			columns_[column].erase(columns_[column].begin() + row);
		--row_count_;

		for (typename std::unordered_map<std::string, std::size_t>::iterator it =
				row_indices_.begin(); it != row_indices_.end();) {
			if (it->second == row)
				it = row_indices_.erase(it);
			else {
				if (it->second > row)
					--it->second;
				++it;
			}
		}
	}

	void RemoveRow(const std::string& name) {
		RemoveRow(RowIndex(name));
	}

	std::vector<T>& GetColumn(std::size_t column) {
		if (column >= columns_.size())
			throw std::out_of_range("Dataset column index out of range");
		return columns_[column];
	}

	const std::vector<T>& GetColumn(std::size_t column) const {
		if (column >= columns_.size())
			throw std::out_of_range("Dataset column index out of range");
		return columns_[column];
	}

	std::vector<T>& GetColumn(const std::string& name) {
		return GetColumn(ColumnIndex(name));
	}

	const std::vector<T>& GetColumn(const std::string& name) const {
		return GetColumn(ColumnIndex(name));
	}

	T& At(std::size_t row, std::size_t column) {
		CheckIndices(row, column);
		return columns_[column][row];
	}

	const T& At(std::size_t row, std::size_t column) const {
		CheckIndices(row, column);
		return columns_[column][row];
	}

	T& At(std::size_t row, const std::string& column) {
		return At(row, ColumnIndex(column));
	}

	const T& At(std::size_t row, const std::string& column) const {
		return At(row, ColumnIndex(column));
	}

	T& At(const std::string& row, std::size_t column) {
		return At(RowIndex(row), column);
	}

	const T& At(const std::string& row, std::size_t column) const {
		return At(RowIndex(row), column);
	}

	T& At(const std::string& row, const std::string& column) {
		return At(RowIndex(row), ColumnIndex(column));
	}

	const T& At(const std::string& row, const std::string& column) const {
		return At(RowIndex(row), ColumnIndex(column));
	}

	T& operator()(std::size_t row, std::size_t column) {
		return At(row, column);
	}

	const T& operator()(std::size_t row, std::size_t column) const {
		return At(row, column);
	}

	T& operator()(const std::string& row, const std::string& column) {
		return At(row, column);
	}

	const T& operator()(const std::string& row,
						const std::string& column) const {
		return At(row, column);
	}

	std::size_t RowIndex(const std::string& name) const {
		return FindIndex(name, row_indices_);
	}

	std::size_t ColumnIndex(const std::string& name) const {
		return FindIndex(name, column_indices_);
	}

private:
	template <typename Map>
	static void CheckNewName(const std::string& name, const Map& indices) {
		if (!name.empty() && indices.find(name) != indices.end())
			throw std::invalid_argument("Duplicate dataset name");
	}

	template <typename Map>
	static std::size_t FindIndex(const std::string& name, const Map& indices) {
		typename Map::const_iterator it = indices.find(name);
		if (it == indices.end())
			throw std::out_of_range("Unknown dataset name");
		return it->second;
	}

	void CheckIndices(std::size_t row, std::size_t column) const {
		if (row >= row_count_ || column >= columns_.size())
			throw std::out_of_range("Dataset index out of range");
	}

	std::vector<std::vector<T> > columns_;
	std::size_t row_count_;
	std::unordered_map<std::string, std::size_t> row_indices_;
	std::unordered_map<std::string, std::size_t> column_indices_;
};

#endif // FTPAT_DATASET_HPP
