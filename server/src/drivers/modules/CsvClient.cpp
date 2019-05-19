#include <fstream>
#include <sstream>

#include "CsvClient.h"

/**
 * @brief Internal class helper to load CSV file to hash
 */
static void loadCsvFile(CCsvClient::hash_t & hash,
                        const char * fileName,
                        const uint8_t primaryIdx,
                        const uint8_t fieldsNumber,
                        const char delimiter = ',')
{
  using std::getline;

  if (!fileName)
  {
    throw CCsvClient::error("not specified the path to CSV file");
  }
  if (0 == fieldsNumber)
  {
    throw CCsvClient::error("invalid value for CSV values in the row");
  }

  // Opening CSV file stream
  std::ifstream csvInStream(fileName);
  if (!csvInStream.is_open())
  {
    throw CCsvClient::error("could not open file: " + string(fileName));
  }

  // CSV file parsing
  string line;
  size_t lineCnt = 0;

  while (getline(csvInStream, line) && ++lineCnt)
  {
    if (line.empty())
    {
      // Skip empty lines
      continue;
    }

    // Parsing row from the file
    std::stringstream lineStream(line);
    vector<string> csvField(fieldsNumber);
    size_t validityCnt = 0;
    for (size_t idx = 0; idx < fieldsNumber; ++idx)
    {
      if (!getline(lineStream, csvField[idx], delimiter))
      {
        //FIXME: unexpected format field %idx at line %lineCnt
        throw CCsvClient::error("unexpected line format");
      }
      if (!csvField[idx].empty())
      {
        ++validityCnt;
      }
    }

    // Validate parsed results
    size_t requiredFieldsCnt = fieldsNumber - 1;
    if (validityCnt < requiredFieldsCnt)
    {
      //FIXME: required at least %validityCnt valid fields in the line %lineCnt
      throw CCsvClient::error("empty mandatory fields");
    }

    const auto it = hash.find(csvField[primaryIdx]);
    if (hash.cend() == it)
    {

      hash.emplace(csvField[primaryIdx],
                   CCsvClient::SRow(lineCnt, csvField));
    }
  }
}

/**
 * @brief Class constructor
 *
 * @param[in] filePath      The CSV file path for parsing and loading
 * @param[in] primaryField  The primary field used as key in the hash
 * @param[in] fieldsNumber  The CSV raw size
 */
CCsvClient::CCsvClient(const char * filePath,
                       const uint8_t primaryField,
                       const uint8_t fieldsNumber)
{
  loadCsvFile(mHash, filePath, primaryField, fieldsNumber);
}

/**
 * @brief Executing number searching in the memory hash table
 *
 * @param[in] number  The number for processing request
 *
 * @return CSV raw data as array with values
 */
const CCsvClient::row_t * CCsvClient::perform(const char * number)
{
  if (!number)
  {
    throw error("not specified number for searching");
  }

  const auto iter = mHash.find(number);
  if (mHash.cend() == iter)
  {
    return nullptr;
  }

  return &iter->second;
}

/**
 * @brief Overloaded method for number searching in the memory hash table
 *
 * @see CHttpClient::perform
 */
const CCsvClient::row_t * CCsvClient::perform(const string & number)
{
  return perform(number.c_str());
}
