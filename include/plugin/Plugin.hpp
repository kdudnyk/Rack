#pragma once
#include "common.hpp"
#include <jansson.h>
#include <vector>


namespace rack {
namespace plugin {


struct Model;


// Subclass this and return a pointer to a new one when init() is called
struct Plugin {
	/** A list of the models available by this plugin, add with addModel() */
	std::vector<Model*> models;
	/** The file path of the plugin's directory */
	std::string path;
	/** OS-dependent library handle */
	void *handle = NULL;

	/** Must be unique. Used for saving patches. Never change this.
	To guarantee uniqueness, it is a good idea to prefix the slug by your "company name" if available, e.g. "MyCompany-MyPlugin"
	*/
	std::string slug;
	/** Your plugin's latest version, using the guidelines at https://github.com/VCVRack/Rack/issues/266. Do not include the "v" prefix.
	*/
	std::string version;
	/** Human-readable display name for your plugin. You can change this on a whim, unlike slugs.
	*/
	std::string name;
	/** The license type of your plugin. Use "proprietary" if all rights are reserved. If your license is in the [SPDX license list](https://spdx.org/licenses/), use its abbreviation in the "Identifier" column.
	*/
	std::string license;
	/** Your name, company, alias, or GitHub username.
	*/
	std::string author;
	/** Your email address for support inquiries.
	*/
	std::string authorEmail;
	/** Homepage featuring the plugin itself.
	*/
	std::string pluginUrl;
	/** Homepage of the author.
	*/
	std::string authorUrl;
	/** The manual of your plugin. HTML, PDF, or GitHub readme/wiki are fine.
	*/
	std::string manualUrl;
	/** The source code homepage. E.g. GitHub repo.
	*/
	std::string sourceUrl;
	/** Link to donation page for users who wish to donate. E.g. PayPal URL.
	*/
	std::string donateUrl;

	virtual ~Plugin();
	void addModel(Model *model);
	Model *getModel(std::string slug);
	void fromJson(json_t *rootJ);
};


} // namespace plugin
} // namespace rack
