/**
@brief Metadata cache system for tracking file extraction and dependencies
*/
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <optional>
#include "json.h"

/**
@brief Metadata about a single extracted file
*/
struct FileMetadata {
  std::string source_path;                    ///< Path to source file
  std::string content_hash;                   ///< SHA-256 hash of file content
  std::string last_modified_str;              ///< Last modification time as string
  std::vector<std::string> generated_files;   ///< List of generated markdown files
  std::vector<std::string> dependencies;     ///< Files this depends on
  size_t construct_count;                     ///< Number of constructs extracted
  std::string language;                       ///< Language used for extraction
  
  // Default constructor to make it work with std::unordered_map
  FileMetadata() = default;
};

/**
@brief Cache entry for tracking extraction metadata
*/
struct CacheEntry {
  std::string version;                                           ///< Cache format version
  std::chrono::system_clock::time_point last_updated;          ///< Cache last update time
  std::unordered_map<std::string, FileMetadata> files;         ///< File path -> metadata mapping
  std::unordered_map<std::string, std::string> output_to_source; ///< Output file -> source file mapping
};

/**
@brief Metadata cache manager for documentation extraction
*/
class DocumentationCache {
  public:
    /**
    @brief Constructor with cache file path
    @param cache_file_path Path to cache file (JSON format)
    */
    explicit DocumentationCache(const std::string& cache_file_path);

    /**
    @brief Load cache from disk
    @return True if cache was loaded successfully, false if not found or corrupted
    */
    bool load();

    /**
    @brief Save cache to disk
    @return True if cache was saved successfully
    */
    bool save();

    /**
    @brief Save cache to disk immediately (for incremental updates)
    @return True if cache was saved successfully
    */
    bool saveImmediately();

    /**
    @brief Check if a file needs extraction based on cache
    @param source_path Path to source file
    @return True if file needs extraction (new, modified, or dependencies changed)
    */
    bool needsExtraction(const std::string& source_path);

    /**
    @brief Update cache entry for a processed file
    @param source_path Path to source file
    @param generated_files List of generated markdown files
    @param construct_count Number of constructs extracted
    @param language Language used for extraction
    */
    void updateFile(const std::string& source_path, 
                   const std::vector<std::string>& generated_files,
                   size_t construct_count,
                   const std::string& language);

    /**
    @brief Remove a file from cache (when source file is deleted)
    @param source_path Path to source file
    */
    void removeFile(const std::string& source_path);

    /**
    @brief Get list of orphaned output files (generated from deleted sources)
    @return Vector of output file paths that should be cleaned up
    */
    std::vector<std::string> getOrphanedFiles();

    /**
    @brief Get list of orphaned files in extract directory (files not in cache)
    @param extract_dir Directory containing generated files
    @return Vector of file paths that are not tracked by cache
    */
    std::vector<std::string> getOrphanedFilesInDirectory(const std::string& extract_dir);

    /**
    @brief Remove orphaned files and update cache
    @param extract_dir Directory containing generated files
    @param dry_run If true, only report what would be deleted without actually deleting
    @return Number of files removed/would be removed
    */
    size_t pruneOrphanedFiles(const std::string& extract_dir, bool dry_run = false);

    /**
    @brief Get cache statistics
    @return Pair of (total_files, total_generated_files)
    */
    std::pair<size_t, size_t> getStats() const;

    /**
    @brief Calculate integrity hash of current directory state
    @param extract_dir Directory containing generated files
    @return Hash representing current state of generated files
    */
    std::string calculateDirectoryHash(const std::string& extract_dir) const;

    /**
    @brief Verify cache integrity against directory state
    @param extract_dir Directory containing generated files
    @return True if cache matches directory state
    */
    bool verifyIntegrity(const std::string& extract_dir) const;

    /**
    @brief Clear all cache data
    */
    void clear();

  private:
    std::string cache_file_path_;  ///< Path to cache file
    CacheEntry cache_;             ///< In-memory cache data

    /**
    @brief Calculate content hash for a file
    @param file_path Path to file
    @return SHA-256 hash string, or empty string on error
    */
    std::string calculateFileHash(const std::string& file_path) const;

    /**
    @brief Convert cache entry to JSON
    @return JSON string representation
    */
    std::string cacheToJson() const;

    /**
    @brief Load cache entry from JSON
    @param json_str JSON string
    @return True if loaded successfully
    */
    bool cacheFromJson(const std::string& json_str);
};