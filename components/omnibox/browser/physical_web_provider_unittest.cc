// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/physical_web_provider.h"

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/physical_web/data_source/physical_web_data_source.h"
#include "components/physical_web/data_source/physical_web_listener.h"
#include "grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

// A mock implementation of the Physical Web data source that allows setting
// metadata for nearby URLs directly.
class MockPhysicalWebDataSource : public PhysicalWebDataSource {
 public:
  MockPhysicalWebDataSource() : metadata_(new base::ListValue()) {}
  ~MockPhysicalWebDataSource() override {}

  void StartDiscovery(bool network_request_enabled) override {}
  void StopDiscovery() override {}

  std::unique_ptr<base::ListValue> GetMetadata() override {
    return metadata_->CreateDeepCopy();
  }

  bool HasUnresolvedDiscoveries() override {
    return false;
  }

  void RegisterListener(PhysicalWebListener* physical_web_listener) override {}

  void UnregisterListener(
      PhysicalWebListener* physical_web_listener) override {}

  // for testing
  void SetMetadata(std::unique_ptr<base::ListValue> metadata) {
    metadata_ = std::move(metadata);
  }

 private:
  std::unique_ptr<base::ListValue> metadata_;
};

// An autocomplete provider client that embeds the mock Physical Web data
// source.
class FakeAutocompleteProviderClient
    : public testing::NiceMock<MockAutocompleteProviderClient> {
 public:
  FakeAutocompleteProviderClient()
      : physical_web_data_source_(base::MakeUnique<MockPhysicalWebDataSource>())
  {
  }

  const AutocompleteSchemeClassifier& GetSchemeClassifier() const override {
    return scheme_classifier_;
  }

  PhysicalWebDataSource* GetPhysicalWebDataSource() override {
    return physical_web_data_source_.get();
  }

  // Convenience method to avoid downcasts when accessing the mock data source.
  MockPhysicalWebDataSource* GetMockPhysicalWebDataSource() {
    return physical_web_data_source_.get();
  }

 private:
  std::unique_ptr<MockPhysicalWebDataSource> physical_web_data_source_;
  TestSchemeClassifier scheme_classifier_;
};

class PhysicalWebProviderTest : public testing::Test {
 protected:
  PhysicalWebProviderTest() : provider_(NULL) {}
  ~PhysicalWebProviderTest() override {}

  void SetUp() override {
    client_.reset(new FakeAutocompleteProviderClient());
    provider_ = PhysicalWebProvider::Create(client_.get(), nullptr);
  }

  void TearDown() override {
    provider_ = NULL;
  }

  // Create a dummy metadata list with |metadata_count| items. Each item is
  // populated with a unique scanned URL and page metadata.
  static std::unique_ptr<base::ListValue> CreateMetadata(
      size_t metadata_count) {
    auto metadata_list = base::MakeUnique<base::ListValue>();
    for (size_t i = 0; i < metadata_count; ++i) {
      std::string item_id = base::SizeTToString(i);
      std::string url = "https://example.com/" + item_id;
      auto metadata_item = base::MakeUnique<base::DictionaryValue>();
      metadata_item->SetString("scannedUrl", url);
      metadata_item->SetString("resolvedUrl", url);
      metadata_item->SetString("icon", url);
      metadata_item->SetString("title", "Example title " + item_id);
      metadata_item->SetString("description", "Example description " + item_id);
      metadata_list->Append(std::move(metadata_item));
    }
    return metadata_list;
  }

  // Construct an AutocompleteInput to represent tapping the omnibox from the
  // new tab page.
  static AutocompleteInput CreateInputForNTP() {
    return AutocompleteInput(
        base::string16(), base::string16::npos, std::string(), GURL(),
        metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
        false, false, true, true, true, TestSchemeClassifier());
  }

  // Construct an AutocompleteInput to represent tapping the omnibox with |url|
  // as the current web page.
  static AutocompleteInput CreateInputWithCurrentUrl(const std::string& url) {
    return AutocompleteInput(base::ASCIIToUTF16(url), base::string16::npos,
                             std::string(), GURL(url),
                             metrics::OmniboxEventProto::OTHER, false, false,
                             true, true, true, TestSchemeClassifier());
  }

  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<PhysicalWebProvider> provider_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PhysicalWebProviderTest);
};

TEST_F(PhysicalWebProviderTest, TestEmptyMetadataListCreatesNoMatches) {
  MockPhysicalWebDataSource* data_source =
      client_->GetMockPhysicalWebDataSource();
  EXPECT_TRUE(data_source);

  data_source->SetMetadata(CreateMetadata(0));

  // Run the test with no text in the omnibox input to simulate NTP.
  provider_->Start(CreateInputForNTP(), false);
  EXPECT_TRUE(provider_->matches().empty());

  // Run the test again with a URL in the omnibox input.
  provider_->Start(CreateInputWithCurrentUrl("http://www.cnn.com"), false);
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(PhysicalWebProviderTest, TestSingleMetadataItemCreatesOneMatch) {
  MockPhysicalWebDataSource* data_source =
      client_->GetMockPhysicalWebDataSource();
  EXPECT_TRUE(data_source);

  // Extract the URL and title before inserting the metadata into the data
  // source.
  std::unique_ptr<base::ListValue> metadata_list = CreateMetadata(1);
  base::DictionaryValue* metadata_item;
  EXPECT_TRUE(metadata_list->GetDictionary(0, &metadata_item));
  std::string resolved_url;
  EXPECT_TRUE(metadata_item->GetString("resolvedUrl", &resolved_url));
  std::string title;
  EXPECT_TRUE(metadata_item->GetString("title", &title));

  data_source->SetMetadata(std::move(metadata_list));

  // Run the test with no text in the omnibox input to simulate NTP.
  provider_->Start(CreateInputForNTP(), false);

  // Check that there is only one match item and its fields are correct.
  EXPECT_EQ(1U, provider_->matches().size());
  const AutocompleteMatch& metadata_match = provider_->matches().front();
  EXPECT_EQ(AutocompleteMatchType::PHYSICAL_WEB, metadata_match.type);
  EXPECT_EQ(resolved_url, metadata_match.destination_url.spec());
  EXPECT_EQ(resolved_url, base::UTF16ToASCII(metadata_match.contents));
  EXPECT_EQ(title, base::UTF16ToASCII(metadata_match.description));
  EXPECT_FALSE(metadata_match.allowed_to_be_default_match);

  // Run the test again with a URL in the omnibox input. An additional match
  // should be added as a default match.
  provider_->Start(CreateInputWithCurrentUrl("http://www.cnn.com"), false);

  size_t metadata_match_count = 0;
  size_t default_match_count = 0;
  for (const auto& match : provider_->matches()) {
    if (match.type == AutocompleteMatchType::PHYSICAL_WEB) {
      EXPECT_EQ(resolved_url, match.destination_url.spec());
      EXPECT_EQ(resolved_url, base::UTF16ToASCII(match.contents));
      EXPECT_EQ(title, base::UTF16ToASCII(match.description));
      EXPECT_FALSE(match.allowed_to_be_default_match);
      ++metadata_match_count;
    } else {
      EXPECT_TRUE(match.allowed_to_be_default_match);
      ++default_match_count;
    }
  }
  EXPECT_EQ(2U, provider_->matches().size());
  EXPECT_EQ(1U, metadata_match_count);
  EXPECT_EQ(1U, default_match_count);
}

TEST_F(PhysicalWebProviderTest, TestManyMetadataItemsCreatesOverflowItem) {
  // This test is intended to verify that an overflow item is created when the
  // number of nearby Physical Web URLs exceeds the maximum allowable matches
  // for this provider. The actual limit for the PhysicalWebProvider may be
  // changed in the future, so create enough metadata to exceed the
  // AutocompleteProvider's limit.
  const size_t metadata_count = AutocompleteProvider::kMaxMatches + 1;

  MockPhysicalWebDataSource* data_source =
      client_->GetMockPhysicalWebDataSource();
  EXPECT_TRUE(data_source);

  data_source->SetMetadata(CreateMetadata(metadata_count));

  {
    // Run the test with no text in the omnibox input to simulate NTP.
    provider_->Start(CreateInputForNTP(), false);

    const size_t match_count = provider_->matches().size();
    EXPECT_LT(match_count, metadata_count);

    // Check that the overflow item is present and its fields are correct. There
    // may be additional match items, but none should be marked as default.
    size_t overflow_match_count = 0;
    for (const auto& match : provider_->matches()) {
      if (match.type == AutocompleteMatchType::PHYSICAL_WEB_OVERFLOW) {
        EXPECT_EQ("chrome://physical-web/", match.destination_url.spec());
        EXPECT_EQ("chrome://physical-web/", base::UTF16ToASCII(match.contents));
        const size_t metadata_matches = match_count - 1;
        std::string description = l10n_util::GetPluralStringFUTF8(
            IDS_PHYSICAL_WEB_OVERFLOW, metadata_count - metadata_matches);
        EXPECT_EQ(description, base::UTF16ToASCII(match.description));
        ++overflow_match_count;
      }
      EXPECT_FALSE(match.allowed_to_be_default_match);
    }
    EXPECT_EQ(1U, overflow_match_count);
  }

  {
    // Run the test again with a URL in the omnibox input. An additional match
    // should be added as a default match.
    provider_->Start(CreateInputWithCurrentUrl("http://www.cnn.com"), false);

    const size_t match_count = provider_->matches().size();
    EXPECT_LT(match_count - 1, metadata_count);

    // Check that the overflow item and default match are present and their
    // fields are correct.
    size_t overflow_match_count = 0;
    size_t default_match_count = 0;
    for (const auto& match : provider_->matches()) {
      if (match.type == AutocompleteMatchType::PHYSICAL_WEB_OVERFLOW) {
        EXPECT_EQ("chrome://physical-web/", match.destination_url.spec());
        EXPECT_EQ("chrome://physical-web/", base::UTF16ToASCII(match.contents));
        const size_t metadata_matches = match_count - 2;
        std::string description = l10n_util::GetPluralStringFUTF8(
            IDS_PHYSICAL_WEB_OVERFLOW, metadata_count - metadata_matches);
        EXPECT_EQ(description, base::UTF16ToASCII(match.description));
        EXPECT_FALSE(match.allowed_to_be_default_match);
        ++overflow_match_count;
      } else if (match.allowed_to_be_default_match) {
        ++default_match_count;
      }
    }
    EXPECT_EQ(1U, overflow_match_count);
    EXPECT_EQ(1U, default_match_count);
  }
}

TEST_F(PhysicalWebProviderTest, TestNoMatchesWithUserInput) {
  MockPhysicalWebDataSource* data_source =
      client_->GetMockPhysicalWebDataSource();
  EXPECT_TRUE(data_source);

  data_source->SetMetadata(CreateMetadata(1));

  // Construct an AutocompleteInput to simulate user input in the omnibox input
  // field. The provider should not generate any matches.
  std::string text("user input");
  const AutocompleteInput input(base::ASCIIToUTF16(text), text.length(),
      std::string(), GURL(),
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
      true, false, true, true, false, TestSchemeClassifier());
  provider_->Start(input, false);

  EXPECT_TRUE(provider_->matches().empty());
}

}
