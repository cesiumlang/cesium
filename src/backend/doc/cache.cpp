/**
@brief Implementation of metadata cache system for documentation extraction
*/
#include <backend/doc/cache.h>
#include <backend/core/cli_utils.h>
#include <backend/core/json.h>
#include <filesystem>
#include <fstream>
#include <sstream>
// #include <iomanip>
#include <set>
#include <algorithm>

// For SHA-256 hashing - using a simple implementation
#include <array>

namespace {
  // Simple hash function for content - using std::hash for now
  // In production, would use a proper SHA-256 implementation
  std::string simpleHash(const std::string& content) {
    auto hash = std::hash<std::string>{}(content);
    std::stringstream ss;
    ss << std::hex << hash;
    return ss.str();
  }

  // Convert file_time_type to string for JSON serialization
  std::string fileTimeToString(const std::filesystem::file_time_type& ftime) {
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    auto time_t = std::chrono::system_clock::to_time_t(sctp);
    std::stringstream ss;
    ss << time_t;
    return ss.str();
  }

  // // Convert string back to file_time_type
  // std::filesystem::file_time_type stringToFileTime(const std::string& str) {
  //   std::time_t time_t_val = std::stoll(str);
  //   auto sctp = std::chrono::system_clock::from_time_t(time_t_val);
  //   return std::filesystem::file_time_type::clock::now() + (sctp - std::chrono::system_clock::now());
  // }
}

DocumentationCache::DocumentationCache(const std::string& cache_file_path)
  : cache_file_path_(cache_file_path) {
  cache_.version = "1.0";
  cache_.last_updated = std::chrono::system_clock::now();
}

bool DocumentationCache::load() {
  CLILogger::debug("DocumentationCache::load: Attempting to load cache from: " + cache_file_path_);
  
  try {
    if (!std::filesystem::exists(cache_file_path_)) {
      CLILogger::debug("DocumentationCache::load: Cache file does not exist: " + cache_file_path_);
      return false;
    }
    
    CLILogger::debug("DocumentationCache::load: Cache file exists, loading...");

    std::ifstream file(cache_file_path_);
    if (!file.is_open()) {
      CLILogger::error("DocumentationCache::load: Failed to open cache file: " + cache_file_path_);
      return false;
    }
    
    CLILogger::debug("DocumentationCache::load: Successfully opened cache file for reading");

    std::string json_content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    file.close();
    
    CLILogger::debug("DocumentationCache::load: Read " + std::to_string(json_content.length()) + " bytes from cache file");

    if (cacheFromJson(json_content)) {
      CLILogger::debug("DocumentationCache::load: Successfully loaded cache with " + std::to_string(cache_.files.size()) + " entries");
      return true;
    } else {
      CLILogger::error("DocumentationCache::load: Failed to parse cache file: " + cache_file_path_);
      return false;
    }
  } catch (const std::exception& e) {
    CLILogger::error("DocumentationCache::load: Exception loading cache: " + std::string(e.what()));
    return false;
  }
}

bool DocumentationCache::save() {
  return saveImmediately();
}

bool DocumentationCache::saveImmediately() {
  CLILogger::debug("DocumentationCache::saveImmediately: Saving cache to: " + cache_file_path_);
  
  try {
    // Create directory if it doesn't exist
    std::filesystem::path cache_path(cache_file_path_);
    if (cache_path.has_parent_path()) {
      std::string parent_dir = cache_path.parent_path().string();
      CLILogger::debug("DocumentationCache::saveImmediately: Creating parent directories for cache file: " + parent_dir);
      try {
        bool created = std::filesystem::create_directories(cache_path.parent_path());
        if (created) {
          CLILogger::debug("DocumentationCache::saveImmediately: Successfully created parent directories: " + parent_dir);
        } else {
          CLILogger::debug("DocumentationCache::saveImmediately: Parent directories already exist: " + parent_dir);
        }
      } catch (const std::filesystem::filesystem_error& e) {
        CLILogger::error("DocumentationCache::saveImmediately: Failed to create parent directories '" + parent_dir + "': " + e.what());
        return false;
      }
    }

    std::ofstream file(cache_file_path_);
    if (!file.is_open()) {
      CLILogger::error("DocumentationCache::saveImmediately: Failed to create cache file: " + cache_file_path_);
      return false;
    }
    
    CLILogger::debug("DocumentationCache::saveImmediately: Successfully opened cache file for writing");

    cache_.last_updated = std::chrono::system_clock::now();
    std::string json_content = cacheToJson();
    CLILogger::debug("DocumentationCache::saveImmediately: Generated JSON content (" + std::to_string(json_content.length()) + " bytes)");
    
    file << json_content;
    if (!file.good()) {
      CLILogger::error("DocumentationCache::saveImmediately: File write errors occurred");
      file.close();
      return false;
    }
    file.close();

    CLILogger::debug("DocumentationCache::saveImmediately: Successfully saved cache with " + std::to_string(cache_.files.size()) + " entries");
    return true;
  } catch (const std::exception& e) {
    CLILogger::error("DocumentationCache::saveImmediately: Exception saving cache: " + std::string(e.what()));
    return false;
  }
}

bool DocumentationCache::needsExtraction(const std::string& source_path) {
  CLILogger::debug("DocumentationCache::needsExtraction: Checking if file needs extraction: " + source_path);
  
  try {
    // Check if file exists
    if (!std::filesystem::exists(source_path)) {
      CLILogger::debug("DocumentationCache::needsExtraction: Source file does not exist: " + source_path);
      return false; // Can't extract non-existent file
    }

    // Check if we have cache entry for this file
    auto it = cache_.files.find(source_path);
    if (it == cache_.files.end()) {
      CLILogger::debug("DocumentationCache::needsExtraction: File not in cache, needs extraction: " + source_path);
      return true; // New file
    }

    const FileMetadata& metadata = it->second;
    CLILogger::debug("DocumentationCache::needsExtraction: Found cache entry for file, checking if up to date");

    // Check if file has been modified
    auto current_time = std::filesystem::last_write_time(source_path);
    std::string current_time_str = fileTimeToString(current_time);
    if (current_time_str != metadata.last_modified_str) {
      CLILogger::debug("DocumentationCache::needsExtraction: File timestamp changed, needs extraction: " + source_path + " (was: " + metadata.last_modified_str + ", now: " + current_time_str + ")");
      return true;
    }

    // Check content hash
    std::string current_hash = calculateFileHash(source_path);
    if (current_hash != metadata.content_hash) {
      CLILogger::debug("DocumentationCache::needsExtraction: File content changed, needs extraction: " + source_path + " (hash changed from " + metadata.content_hash + " to " + current_hash + ")");
      return true;
    }

    // Check if any generated files are missing
    for (const auto& generated_file : metadata.generated_files) {
      if (!std::filesystem::exists(generated_file)) {
        CLILogger::debug("DocumentationCache::needsExtraction: Generated file missing, needs extraction: " + generated_file);
        return true;
      }
    }

    // File is up to date
    CLILogger::debug("DocumentationCache::needsExtraction: File is up to date, no extraction needed: " + source_path);
    return false;
  } catch (const std::exception& e) {
    CLILogger::error("DocumentationCache::needsExtraction: Exception checking file for extraction: " + std::string(e.what()));
    return true; // When in doubt, extract
  }
}

void DocumentationCache::updateFile(const std::string& source_path,
                                   const std::vector<std::string>& generated_files,
                                   size_t construct_count,
                                   const std::string& language) {
  CLILogger::debug("DocumentationCache::updateFile: Updating cache entry for: " + source_path + " (" + std::to_string(construct_count) + " constructs, " + std::to_string(generated_files.size()) + " files, language: " + language + ")");
  
  try {
    FileMetadata metadata;
    metadata.source_path = source_path;
    
    CLILogger::debug("DocumentationCache::updateFile: Calculating file hash for: " + source_path);
    metadata.content_hash = calculateFileHash(source_path);
    CLILogger::debug("DocumentationCache::updateFile: File hash: " + metadata.content_hash);
    
    metadata.last_modified_str = fileTimeToString(std::filesystem::last_write_time(source_path));
    metadata.generated_files = generated_files;
    metadata.construct_count = construct_count;
    metadata.language = language;

    // Update main mapping
    cache_.files[source_path] = metadata;

    // Update reverse mapping for orphan detection
    for (const auto& generated_file : generated_files) {
      cache_.output_to_source[generated_file] = source_path;
    }

    CLILogger::debug("DocumentationCache::updateFile: Successfully updated cache entry for: " + source_path +
                    " (" + std::to_string(generated_files.size()) + " files generated)");
  } catch (const std::exception& e) {
    CLILogger::error("DocumentationCache::updateFile: Exception updating cache for file: " + std::string(e.what()));
  }
}

void DocumentationCache::removeFile(const std::string& source_path) {
  auto it = cache_.files.find(source_path);
  if (it != cache_.files.end()) {
    // Remove reverse mappings
    for (const auto& generated_file : it->second.generated_files) {
      cache_.output_to_source.erase(generated_file);
    }

    // Remove main entry
    cache_.files.erase(it);

    CLILogger::debug("Removed cache entry for: " + source_path);
  }
}

std::vector<std::string> DocumentationCache::getOrphanedFiles() {
  std::vector<std::string> orphaned;

  for (const auto& [output_file, source_file] : cache_.output_to_source) {
    // Check if source file still exists
    if (!std::filesystem::exists(source_file)) {
      // Check if output file exists (might have been manually deleted)
      if (std::filesystem::exists(output_file)) {
        orphaned.push_back(output_file);
      }
    }
  }

  return orphaned;
}

std::vector<std::string> DocumentationCache::getOrphanedFilesInDirectory(const std::string& extract_dir) {
  std::vector<std::string> orphaned;

  if (!std::filesystem::exists(extract_dir)) {
    return orphaned;
  }

  // Build set of all files that should exist according to cache
  std::set<std::string> cached_files;
  for (const auto& [source_path, metadata] : cache_.files) {
    for (const auto& generated_file : metadata.generated_files) {
      std::filesystem::path p(generated_file);
      cached_files.insert(p.filename().string());
    }
  }

  // Add the cache file itself to the list of expected files
  cached_files.insert(".cesium-cache.json");

  // Check for files in directory that aren't in cache
  for (const auto& entry : std::filesystem::directory_iterator(extract_dir)) {
    if (entry.is_regular_file()) {
      std::string filename = entry.path().filename().string();
      // Skip log files and other non-documentation files
      if (filename.ends_with(".md") && cached_files.find(filename) == cached_files.end()) {
        orphaned.push_back(entry.path().string());
      }
    }
  }

  return orphaned;
}

size_t DocumentationCache::pruneOrphanedFiles(const std::string& extract_dir, bool dry_run) {
  // First, get orphaned files from deleted sources
  std::vector<std::string> source_orphaned = getOrphanedFiles();

  // Second, get orphaned files in directory
  std::vector<std::string> directory_orphaned = getOrphanedFilesInDirectory(extract_dir);

  size_t total_orphaned = source_orphaned.size() + directory_orphaned.size();
  size_t files_removed = 0;

  // Remove files from deleted sources
  for (const auto& file_path : source_orphaned) {
    if (dry_run) {
      CLILogger::info("Would remove orphaned file (source deleted): " + file_path);
      files_removed++;
    } else {
      try {
        if (std::filesystem::remove(file_path)) {
          CLILogger::info("Removed orphaned file (source deleted): " + file_path);
          files_removed++;
        }
      } catch (const std::exception& e) {
        CLILogger::warning("Failed to remove orphaned file " + file_path + ": " + e.what());
      }
    }
  }

  // Remove files in directory not tracked by cache
  for (const auto& file_path : directory_orphaned) {
    if (dry_run) {
      CLILogger::info("Would remove orphaned file (not in cache): " + file_path);
      files_removed++;
    } else {
      try {
        if (std::filesystem::remove(file_path)) {
          CLILogger::info("Removed orphaned file (not in cache): " + file_path);
          files_removed++;
        }
      } catch (const std::exception& e) {
        CLILogger::warning("Failed to remove orphaned file " + file_path + ": " + e.what());
      }
    }
  }

  if (!dry_run && files_removed > 0) {
    // Clean up cache entries for removed files
    auto it = cache_.output_to_source.begin();
    while (it != cache_.output_to_source.end()) {
      if (!std::filesystem::exists(it->first)) {
        it = cache_.output_to_source.erase(it);
      } else {
        ++it;
      }
    }

    // Save updated cache
    saveImmediately();
    CLILogger::info("Pruned " + std::to_string(files_removed) + " orphaned files");
  } else if (dry_run && total_orphaned > 0) {
    CLILogger::info("Would prune " + std::to_string(total_orphaned) + " orphaned files");
  }

  return files_removed;
}

std::pair<size_t, size_t> DocumentationCache::getStats() const {
  size_t total_generated = 0;
  for (const auto& [path, metadata] : cache_.files) {
    total_generated += metadata.generated_files.size();
  }
  return {cache_.files.size(), total_generated};
}

void DocumentationCache::clear() {
  cache_.files.clear();
  cache_.output_to_source.clear();
  cache_.last_updated = std::chrono::system_clock::now();
}

std::string DocumentationCache::calculateFileHash(const std::string& file_path) const {
  try {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
      CLILogger::warning("DocumentationCache::calculateFileHash: Failed to open file for hashing: " + file_path);
      return "";
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    file.close();
    
    std::string hash = simpleHash(content);
    CLILogger::debug("DocumentationCache::calculateFileHash: Calculated hash for " + file_path + " (" + std::to_string(content.length()) + " bytes): " + hash);
    return hash;
  } catch (const std::exception& e) {
    CLILogger::error("DocumentationCache::calculateFileHash: Exception calculating hash for file: " + file_path + " - " + e.what());
    return "";
  }
}

std::string DocumentationCache::cacheToJson() const {
  // For now, return a simple JSON structure
  // In a full implementation, would use a proper JSON library
  std::stringstream ss;
  ss << "{\n";
  ss << "  \"version\": \"" << cache_.version << "\",\n";
  ss << "  \"last_updated\": " << std::chrono::duration_cast<std::chrono::seconds>(
      cache_.last_updated.time_since_epoch()).count() << ",\n";
  ss << "  \"file_count\": " << cache_.files.size() << ",\n";
  ss << "  \"files\": {\n";

  bool first = true;
  for (const auto& [path, metadata] : cache_.files) {
    if (!first) ss << ",\n";
    first = false;

    ss << "    \"" << path << "\": {\n";
    ss << "      \"content_hash\": \"" << metadata.content_hash << "\",\n";
    ss << "      \"last_modified\": \"" << metadata.last_modified_str << "\",\n";
    ss << "      \"construct_count\": " << metadata.construct_count << ",\n";
    ss << "      \"language\": \"" << metadata.language << "\",\n";
    ss << "      \"generated_files\": [";

    bool first_file = true;
    for (const auto& gen_file : metadata.generated_files) {
      if (!first_file) ss << ", ";
      first_file = false;
      ss << "\"" << gen_file << "\"";
    }
    ss << "]\n";
    ss << "    }";
  }

  ss << "\n  }\n";
  ss << "}";

  return ss.str();
}

std::string DocumentationCache::calculateDirectoryHash(const std::string& extract_dir) const {
  try {
    std::vector<std::pair<std::string, std::string>> file_times;

    // Collect all .md files with their timestamps
    if (std::filesystem::exists(extract_dir)) {
      for (const auto& entry : std::filesystem::directory_iterator(extract_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".md") {
          auto timestamp = std::filesystem::last_write_time(entry.path());
          std::string time_str = fileTimeToString(timestamp);
          file_times.push_back({entry.path().filename().string(), time_str});
        }
      }
    }

    // Sort for consistent hashing
    std::sort(file_times.begin(), file_times.end());

    // Create hash from sorted filenames and timestamps
    std::string combined;
    for (const auto& [filename, timestamp] : file_times) {
      combined += filename + ":" + timestamp + ";";
    }

    return calculateFileHash("/tmp/hash_content"); // Placeholder - in real implementation would hash the combined string
  } catch (const std::exception& e) {
    CLILogger::error("Failed to calculate directory hash: " + std::string(e.what()));
    return "";
  }
}

bool DocumentationCache::verifyIntegrity(const std::string& extract_dir) const {
  try {
    // Check that all cached generated files still exist
    for (const auto& [source_path, metadata] : cache_.files) {
      for (const auto& generated_file : metadata.generated_files) {
        if (!std::filesystem::exists(generated_file)) {
          CLILogger::warning("Cache integrity issue: Missing generated file: " + generated_file);
          return false;
        }
      }
    }

    // Check for orphaned files in extract directory
    std::set<std::string> cached_files;
    for (const auto& [source_path, metadata] : cache_.files) {
      for (const auto& generated_file : metadata.generated_files) {
        std::filesystem::path p(generated_file);
        cached_files.insert(p.filename().string());
      }
    }

    if (std::filesystem::exists(extract_dir)) {
      for (const auto& entry : std::filesystem::directory_iterator(extract_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".md") {
          std::string filename = entry.path().filename().string();
          if (cached_files.find(filename) == cached_files.end()) {
            CLILogger::warning("Cache integrity issue: Orphaned file: " + filename);
            return false;
          }
        }
      }
    }

    return true;
  } catch (const std::exception& e) {
    CLILogger::error("Failed to verify cache integrity: " + std::string(e.what()));
    return false;
  }
}

bool DocumentationCache::cacheFromJson(const std::string& json_str) {
  CLILogger::debug("DocumentationCache::cacheFromJson: Parsing JSON cache data (" + std::to_string(json_str.length()) + " bytes)");
  
  try {
    // Parse JSON string directly with yyjson
    yyjson_doc* doc = yyjson_read(json_str.c_str(), json_str.length(), 0);
    if (!doc) {
      CLILogger::error("DocumentationCache::cacheFromJson: Failed to parse cache JSON");
      return false;
    }
    
    CLILogger::debug("DocumentationCache::cacheFromJson: Successfully parsed JSON document");

    // Wrap in JsonDoc for easier access
    JsonDoc json_doc(doc);
    const JsonDoc& doc_ref = json_doc;

    // Clear existing cache
    CLILogger::debug("DocumentationCache::cacheFromJson: Clearing existing cache data");
    cache_.files.clear();
    cache_.output_to_source.clear();

    // Parse version
    if (!doc_ref["version"].isNull()) {
      cache_.version = doc_ref["version"].asString();
      CLILogger::debug("DocumentationCache::cacheFromJson: Cache version: " + cache_.version);
    }

    // Parse last_updated
    if (!doc_ref["last_updated"].isNull()) {
      auto timestamp = std::chrono::seconds(doc_ref["last_updated"].asInt());
      cache_.last_updated = std::chrono::system_clock::time_point(timestamp);
    }

    // Parse files
    JsonValue files_obj = doc_ref["files"];
    if (!files_obj.isNull()) {
      CLILogger::debug("DocumentationCache::cacheFromJson: Parsing file entries");
      files_obj.forEachObject([&](const std::string& source_path, JsonValue file_data) {
        CLILogger::debug("DocumentationCache::cacheFromJson: Processing file: " + source_path);
        FileMetadata metadata;
        metadata.source_path = source_path;

        if (!file_data["content_hash"].isNull()) {
          metadata.content_hash = file_data["content_hash"].asString();
        }
        if (!file_data["last_modified"].isNull()) {
          metadata.last_modified_str = file_data["last_modified"].asString();
        }
        if (!file_data["construct_count"].isNull()) {
          metadata.construct_count = file_data["construct_count"].asInt();
        }
        if (!file_data["language"].isNull()) {
          metadata.language = file_data["language"].asString();
        }

        // Parse generated_files array
        JsonValue gen_files = file_data["generated_files"];
        if (!gen_files.isNull()) {
          gen_files.forEachArray([&](size_t index, JsonValue file_path) {
            (void)index; // Unused
            std::string path = file_path.asString();
            metadata.generated_files.push_back(path);
            // Build output_to_source mapping
            cache_.output_to_source[path] = source_path;
          });
        }

        cache_.files[source_path] = metadata;
        CLILogger::debug("DocumentationCache::cacheFromJson: Successfully parsed entry for: " + source_path + " (" + std::to_string(metadata.generated_files.size()) + " generated files)");
      });
    }

    CLILogger::debug("DocumentationCache::cacheFromJson: Successfully parsed cache with " + std::to_string(cache_.files.size()) + " entries");
    return true;
  } catch (const std::exception& e) {
    CLILogger::error("DocumentationCache::cacheFromJson: Exception parsing cache JSON: " + std::string(e.what()));
    return false;
  }
}
