//
// Copyright (C) 2011-2012 Rim Zaidullin <creator@bash.org.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef _COCAINE_DEALER_RESPONSE_HPP_INCLUDED_
#define _COCAINE_DEALER_RESPONSE_HPP_INCLUDED_

#include <string>

#include <boost/shared_ptr.hpp>

#include <cocaine/dealer/forwards.hpp>
#include <cocaine/dealer/structs.hpp>

namespace cocaine {
namespace dealer {

class response {
public:
	explicit response(client* c);
	virtual ~response();

	bool get();
	const void* data();
	size_t size();
	int code();
	std::string error_message();

private:
	friend class client;

	void init(const void* data, size_t size, const message_path& path, const message_policy& policy);

	boost::shared_ptr<response_impl> get_impl();
	boost::shared_ptr<response_impl> impl_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_RESPONSE_HPP_INCLUDED_