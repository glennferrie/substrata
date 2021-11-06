/*=====================================================================
WebServerRequestHandler.cpp
---------------------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "WebServerRequestHandler.h"


#include "WebDataStore.h"
#include "AdminHandlers.h"
#include "MainPageHandlers.h"
#include "WebsiteExcep.h"
#include "Escaping.h"
#include "LoginHandlers.h"
#include "AccountHandlers.h"
#include "ResponseUtils.h"
#include "RequestHandler.h"
#include "ResourceHandlers.h"
#include "PayPalHandlers.h"
#include "CoinbaseHandlers.h"
#include "AuctionHandlers.h"
#include "ScreenshotHandlers.h"
#include "OrderHandlers.h"
#include "ParcelHandlers.h"
#include "../server/WorkerThread.h"
#include "../server/Server.h"
#include <StringUtils.h>
#include <Parser.h>
#include <MemMappedFile.h>
#include <ConPrint.h>
#include <FileUtils.h>
#include <Exception.h>
#include <Lock.h>
#include <WebSocket.h>


WebServerRequestHandler::WebServerRequestHandler()
{}


WebServerRequestHandler::~WebServerRequestHandler()
{}


static bool isLetsEncryptFileQuerySafe(const std::string& s)
{
	for(size_t i=0; i<s.size(); ++i)
		if(!(::isAlphaNumeric(s[i]) || (s[i] == '-') || (s[i] == '_') || (s[i] == '.')))
			return false;
	return true;
}


static bool isFileNameSafe(const std::string& s)
{
	for(size_t i=0; i<s.size(); ++i)
		if(!(::isAlphaNumeric(s[i]) || (s[i] == '-') || (s[i] == '_') || (s[i] == '.')))
			return false;
	return true;
}


void WebServerRequestHandler::handleRequest(const web::RequestInfo& request, web::ReplyInfo& reply_info)
{
	if(!request.tls_connection)
	{
		// Redirect to https (unless the server is running on localhost, which we will allow to use non-https for testing)

		// Find the hostname the request was sent to - look through the headers for 'host'.
		std::string hostname;
		for(size_t i=0; i<request.headers.size(); ++i)
			if(StringUtils::equalCaseInsensitive(request.headers[i].key, "host"))
				hostname = request.headers[i].value.to_string();
		if(hostname != "localhost")
		{
			const std::string response = 
				"HTTP/1.1 301 Redirect\r\n" // 301 = Moved Permanently
				"Location: https://" + hostname + request.path + "\r\n"
				"Content-Length: 0\r\n"
				"\r\n";
			reply_info.socket->writeData(response.c_str(), response.size());
			return;
		}
	}


	if(request.verb == "POST")
	{
		// Route PUT request
		if(request.path == "/login_post")
		{
			LoginHandlers::handleLoginPost(*this->world_state, request, reply_info);
		}
		else if(request.path == "/logout_post")
		{
			LoginHandlers::handleLogoutPost(request, reply_info);
		}
		else if(request.path == "/signup_post")
		{
			LoginHandlers::handleSignUpPost(*this->world_state, request, reply_info);
		}
		else if(request.path == "/reset_password_post")
		{
			LoginHandlers::handleResetPasswordPost(*this->world_state, request, reply_info);
		}
		else if(request.path == "/set_new_password_post")
		{
			LoginHandlers::handleSetNewPasswordPost(*this->world_state, request, reply_info);
		}
		else if(request.path == "/ipn_listener")
		{
			PayPalHandlers::handleIPNPost(*this->world_state, request, reply_info);
		}
		else if(request.path == "/coinbase_webhook")
		{
			CoinbaseHandlers::handleCoinbaseWebhookPost(*this->world_state, request, reply_info);
		}
		else if(request.path == "/buy_parcel_now_paypal")
		{
			AuctionHandlers::handleParcelBuyNowWithPayPal(*this->world_state, request, reply_info);
		}
		else if(request.path == "/buy_parcel_now_coinbase")
		{
			AuctionHandlers::handleParcelBuyNowWithCoinbase(*this->world_state, request, reply_info);
		}
		else if(request.path == "/buy_parcel_with_paypal_post")
		{
			AuctionHandlers::handleBuyParcelWithPayPalPost(*this->world_state, request, reply_info);
		}
		else if(request.path == "/buy_parcel_with_coinbase_post")
		{
			AuctionHandlers::handleBuyParcelWithCoinbasePost(*this->world_state, request, reply_info);
		}
		else if(request.path == "/admin_create_parcel_auction_post")
		{
			AdminHandlers::createParcelAuctionPost(*this->world_state, request, reply_info);
		}
		else if(request.path == "/admin_set_parcel_owner_post")
		{
			AdminHandlers::handleSetParcelOwnerPost(*this->world_state, request, reply_info);
		}
		else if(request.path == "/admin_regenerate_parcel_auction_screenshots")
		{
			AdminHandlers::handleRegenerateParcelAuctionScreenshots(*this->world_state, request, reply_info);
		}
		else if(request.path == "/admin_regenerate_parcel_screenshots")
		{
			AdminHandlers::handleRegenerateParcelScreenshots(*this->world_state, request, reply_info);
		}
		else if(request.path == "/admin_terminate_parcel_auction")
		{
			AdminHandlers::handleTerminateParcelAuction(*this->world_state, request, reply_info);
		}
		else if(request.path == "/admin_mark_parcel_as_nft_minted_post")
		{
			AdminHandlers::handleMarkParcelAsNFTMintedPost(*this->world_state, request, reply_info);
		}
		else if(request.path == "/admin_mark_parcel_as_not_nft_post")
		{
			AdminHandlers::handleMarkParcelAsNotNFTPost(*this->world_state, request, reply_info);
		}
		else if(request.path == "/admin_retry_parcel_mint_post")
		{
			AdminHandlers::handleRetryParcelMintPost(*this->world_state, request, reply_info);
		}
		else if(request.path == "/admin_set_transaction_state_to_new_post")
		{
			AdminHandlers::handleSetTransactionStateToNewPost(*this->world_state, request, reply_info);
		}
		else if(request.path == "/admin_set_transaction_state_hash")
		{
			AdminHandlers::handleSetTransactionHashPost(*this->world_state, request, reply_info);
		}
		else if(request.path == "/admin_set_transaction_nonce")
		{
			AdminHandlers::handleSetTransactionNoncePost(*this->world_state, request, reply_info);
		}
		else if(request.path == "/admin_delete_transaction_post")
		{
			AdminHandlers::handleDeleteTransactionPost(*this->world_state, request, reply_info);
		}
		else if(request.path == "/admin_regen_map_tiles_post")
		{
			AdminHandlers::handleRegenMapTilesPost(*this->world_state, request, reply_info);
		}
		else if(request.path == "/admin_set_min_next_nonce_post")
		{
			AdminHandlers::handleSetMinNextNoncePost(*this->world_state, request, reply_info);
		}
		else if(request.path == "/regenerate_parcel_screenshots")
		{
			ParcelHandlers::handleRegenerateParcelScreenshots(*this->world_state, request, reply_info);
		}
		else if(request.path == "/edit_parcel_description_post")
		{
			ParcelHandlers::handleEditParcelDescriptionPost(*this->world_state, request, reply_info);
		}
		else if(request.path == "/add_parcel_writer_post")
		{
			ParcelHandlers::handleAddParcelWriterPost(*this->world_state, request, reply_info);
		}
		else if(request.path == "/remove_parcel_writer_post")
		{
			ParcelHandlers::handleRemoveParcelWriterPost(*this->world_state, request, reply_info);
		}
		else if(request.path == "/account_eth_sign_message_post")
		{
			AccountHandlers::handleEthSignMessagePost(*this->world_state, request, reply_info);
		}
		else if(request.path == "/make_parcel_into_nft_post")
		{
			AccountHandlers::handleMakeParcelIntoNFTPost(*this->world_state, request, reply_info);
		}
		else if(request.path == "/claim_parcel_owner_by_nft_post")
		{
			AccountHandlers::handleClaimParcelOwnerByNFTPost(*this->world_state, request, reply_info);
		}
		else
		{
			const std::string page = "Unknown post URL";
			web::ResponseUtils::writeHTTPOKHeaderAndData(reply_info, page);
		}
	}
	else if(request.verb == "GET")
	{
		// Route GET request
		if(request.path == "/")
		{
			MainPageHandlers::renderRootPage(*this->world_state, request, reply_info);
		}
		else if(request.path == "/terms")
		{
			MainPageHandlers::renderTermsOfUse(*this->world_state, request, reply_info);
		}
		else if(request.path == "/about_parcel_sales")
		{
			MainPageHandlers::renderAboutParcelSales(*this->world_state, request, reply_info);
		}
		else if(request.path == "/about_scripting")
		{
			MainPageHandlers::renderAboutScripting(*this->world_state, request, reply_info);
		}
		else if(request.path == "/about_substrata")
		{
			MainPageHandlers::renderAboutSubstrataPage(*this->world_state, request, reply_info);
		}
		else if(request.path == "/bot_status")
		{
			MainPageHandlers::renderBotStatusPage(*this->world_state, request, reply_info);
		}
		else if(request.path == "/faq")
		{
			MainPageHandlers::renderFAQ(*this->world_state, request, reply_info);
		}
		else if(request.path == "/map")
		{
			MainPageHandlers::renderMapPage(*this->world_state, request, reply_info);
		}
		else if(request.path == "/pdt_landing")
		{
			PayPalHandlers::handlePayPalPDTOrderLanding(*this->world_state, request, reply_info);
		}
		else if(request.path == "/parcel_auction_list")
		{
			AuctionHandlers::renderParcelAuctionListPage(*this->world_state, request, reply_info);
		}
		else if(request.path == "/recent_parcel_sales")
		{
			AuctionHandlers::renderRecentParcelSalesPage(*this->world_state, request, reply_info);
		}
		else if(::hasPrefix(request.path, "/parcel_auction/")) // parcel auction ID follows in URL
		{
			AuctionHandlers::renderParcelAuctionPage(*this->world_state, request, reply_info);
		}
		else if(::hasPrefix(request.path, "/buy_parcel_with_paypal/")) // parcel ID follows in URL
		{
			AuctionHandlers::renderBuyParcelWithPayPalPage(*this->world_state, request, reply_info);
		}
		else if(::hasPrefix(request.path, "/buy_parcel_with_coinbase/")) // parcel ID follows in URL
		{
			AuctionHandlers::renderBuyParcelWithCoinbasePage(*this->world_state, request, reply_info);
		}
		else if(::hasPrefix(request.path, "/order/")) // Order ID follows in URL
		{
			OrderHandlers::renderOrderPage(*this->world_state, request, reply_info);
		}
		else if(::hasPrefix(request.path, "/parcel/")) // Parcel ID follows in URL
		{
			ParcelHandlers::renderParcelPage(*this->world_state, request, reply_info);
		}
		else if(request.path == "/edit_parcel_description")
		{
			ParcelHandlers::renderEditParcelDescriptionPage(*this->world_state, request, reply_info);
		}
		else if(request.path == "/add_parcel_writer")
		{
			ParcelHandlers::renderAddParcelWriterPage(*this->world_state, request, reply_info);
		}
		else if(request.path == "/remove_parcel_writer")
		{
			ParcelHandlers::renderRemoveParcelWriterPage(*this->world_state, request, reply_info);
		}
		else if(request.path == "/admin")
		{
			AdminHandlers::renderMainAdminPage(*this->world_state, request, reply_info);
		}
		else if(request.path == "/admin_users")
		{
			AdminHandlers::renderUsersPage(*this->world_state, request, reply_info);
		}
		else if(request.path == "/admin_parcels")
		{
			AdminHandlers::renderParcelsPage(*this->world_state, request, reply_info);
		}
		else if(request.path == "/admin_parcel_auctions")
		{
			AdminHandlers::renderParcelAuctionsPage(*this->world_state, request, reply_info);
		}
		else if(request.path == "/admin_orders")
		{
			AdminHandlers::renderOrdersPage(*this->world_state, request, reply_info);
		}
		else if(request.path == "/admin_sub_eth_transactions")
		{
			AdminHandlers::renderSubEthTransactionsPage(*this->world_state, request, reply_info);
		}
		else if(::hasPrefix(request.path, "/admin_sub_eth_transaction/"))
		{
			AdminHandlers::renderAdminSubEthTransactionPage(*this->world_state, request, reply_info);
		}
		else if(request.path == "/admin_map")
		{
			AdminHandlers::renderMapPage(*this->world_state, request, reply_info);
		}
		else if(::hasPrefix(request.path, "/admin_create_parcel_auction/")) // parcel ID follows in URL
		{
			AdminHandlers::renderCreateParcelAuction(*this->world_state, request, reply_info);
		}
		else if(::hasPrefix(request.path, "/admin_set_parcel_owner/")) // parcel ID follows in URL
		{
			AdminHandlers::renderSetParcelOwnerPage(*this->world_state, request, reply_info);
		}
		else if(::hasPrefix(request.path, "/admin_order/")) // order ID follows in URL
		{
			AdminHandlers::renderAdminOrderPage(*this->world_state, request, reply_info);
		}
		else if(request.path == "/login")
		{
			LoginHandlers::renderLoginPage(request, reply_info);
		}
		else if(request.path == "/signup")
		{
			LoginHandlers::renderSignUpPage(request, reply_info);
		}
		else if(request.path == "/reset_password")
		{
			LoginHandlers::renderResetPasswordPage(request, reply_info);
		}
		else if(request.path == "/reset_password_email")
		{
			LoginHandlers::renderResetPasswordFromEmailPage(*this->world_state, request, reply_info);
		}
		else if(request.path == "/account")
		{
			AccountHandlers::renderUserAccountPage(*this->world_state, request, reply_info);
		}
		else if(request.path == "/prove_eth_address_owner")
		{
			AccountHandlers::renderProveEthAddressOwnerPage(*this->world_state, request, reply_info);
		}
		else if(request.path == "/prove_parcel_owner_by_nft")
		{
			AccountHandlers::renderProveParcelOwnerByNFT(*this->world_state, request, reply_info);
		}
		else if(request.path == "/make_parcel_into_nft")
		{
			AccountHandlers::renderMakeParcelIntoNFTPage(*this->world_state, request, reply_info);
		}
		else if(request.path == "/parcel_claim_succeeded")
		{
			AccountHandlers::renderParcelClaimSucceeded(*this->world_state, request, reply_info);
		}
		else if(request.path == "/parcel_claim_failed")
		{
			AccountHandlers::renderParcelClaimFailed(*this->world_state, request, reply_info);
		}
		else if(request.path == "/parcel_claim_invalid")
		{
			AccountHandlers::renderParcelClaimInvalid(*this->world_state, request, reply_info);
		}
		else if(request.path == "/making_parcel_into_nft")
		{
			AccountHandlers::renderMakingParcelIntoNFT(*this->world_state, request, reply_info);
		}
		else if(request.path == "/making_parcel_into_nft_failed")
		{
			AccountHandlers::renderMakingParcelIntoNFTFailed(*this->world_state, request, reply_info);
		}
		else if(::hasPrefix(request.path, "/p/")) // URL for parcel ERC 721 metadata JSON
		{
			ParcelHandlers::renderMetadata(*world_state, request, reply_info);
		}
		else if(::hasPrefix(request.path, "/screenshot/")) // Screenshot ID follows
		{
			ScreenshotHandlers::handleScreenshotRequest(*world_state, *data_store, request, reply_info);
		}
		else if(request.path == "/tile")
		{
			ScreenshotHandlers::handleMapTileRequest(*world_state, *data_store, request, reply_info);
		}
		else if(::hasPrefix(request.path, "/files/"))
		{
			const std::string filename = ::eatPrefix(request.path, "/files/");
			if(isFileNameSafe(filename))
			{
				try
				{
					MemMappedFile file(data_store->public_files_dir + "/" + filename);
					
					const std::string content_type = web::ResponseUtils::getContentTypeForPath(filename);

					web::ResponseUtils::writeHTTPOKHeaderAndDataWithCacheMaxAge(reply_info, file.fileData(), file.fileSize(), content_type, 3600*24*14);
				}
				catch(glare::Exception&)
				{
					web::ResponseUtils::writeHTTPOKHeaderAndData(reply_info, "Failed to load file '" + filename + "'.");
				}
			}
			else
			{
				web::ResponseUtils::writeHTTPOKHeaderAndData(reply_info, "invalid/unsafe filename");
				return;
			}
		}
		else if(::hasPrefix(request.path, "/.well-known/acme-challenge/")) // Support for Let's encrypt: Serve up challenge response file.
		{
			const std::string filename = ::eatPrefix(request.path, "/.well-known/acme-challenge/");
			if(!isLetsEncryptFileQuerySafe(filename))
			{
				web::ResponseUtils::writeHTTPOKHeaderAndData(reply_info, "invalid/unsafe file query");
				return;
			}

			// Serve up the file
			try
			{
				std::string contents;
				FileUtils::readEntireFile(data_store->letsencrypt_webroot + "/.well-known/acme-challenge/" + filename, contents);
				web::ResponseUtils::writeHTTPOKHeaderAndData(reply_info, contents);
			}
			catch(FileUtils::FileUtilsExcep& e)
			{
				web::ResponseUtils::writeHTTPOKHeaderAndData(reply_info, "Failed to load file '" + filename + "': " + e.what());
			}
		}
		else if(::hasPrefix(request.path, "/resource/"))
		{
			ResourceHandlers::handleResourceRequest(*this->world_state, request, reply_info);
		}
		else if(request.path ==  "/list_resources")
		{
			ResourceHandlers::listResources(*this->world_state, request, reply_info);
		}
#if 0
		else if(request.path == "/webclient") // TEMP HACK
		{
			try
			{
				std::string contents;
				FileUtils::readEntireFile("N:\\new_cyberspace\\trunk\\webclient\\client.html", contents);
				web::ResponseUtils::writeHTTPOKHeaderAndData(reply_info, contents);
			}
			catch(FileUtils::FileUtilsExcep& e)
			{
				web::ResponseUtils::writeHTTPOKHeaderAndData(reply_info, e.what());
			}
		}
		else if(request.path == "/three.js") // TEMP HACK
		{
			try
			{
				std::string contents;
				FileUtils::readEntireFile("N:\\new_cyberspace\\trunk\\webclient\\three.js", contents);
				web::ResponseUtils::writeHTTPOKHeaderAndData(reply_info, contents.data(), contents.length(), "text/javascript");
			}
			catch(FileUtils::FileUtilsExcep& e)
			{
				web::ResponseUtils::writeHTTPOKHeaderAndData(reply_info, e.what());
			}
		}
		else if(request.path == "/webclient.js")// TEMP HACK
		{
			try
			{
				std::string contents;
				FileUtils::readEntireFile("N:\\new_cyberspace\\trunk\\webclient\\webclient.js", contents);
				web::ResponseUtils::writeHTTPOKHeaderAndData(reply_info, contents.data(), contents.length(), "text/javascript");
			}
			catch(FileUtils::FileUtilsExcep& e)
			{
				web::ResponseUtils::writeHTTPOKHeaderAndData(reply_info, e.what());
			}
		}
		else if(request.path == "/examples/jsm/loaders/GLTFLoader.js")// TEMP HACK
		{
			try
			{
				std::string contents;
				FileUtils::readEntireFile("N:\\new_cyberspace\\trunk\\webclient\\examples/jsm/loaders/GLTFLoader.js", contents);
				web::ResponseUtils::writeHTTPOKHeaderAndData(reply_info, contents.data(), contents.length(), "text/javascript");
			}
			catch(FileUtils::FileUtilsExcep& e)
			{
				web::ResponseUtils::writeHTTPOKHeaderAndData(reply_info, e.what());
			}
		}
		else if(request.path == "/examples/jsm/objects/Sky.js")// TEMP HACK
		{
			try
			{
				std::string contents;
				FileUtils::readEntireFile("N:\\new_cyberspace\\trunk\\webclient\\examples/jsm/objects/Sky.js", contents);
				web::ResponseUtils::writeHTTPOKHeaderAndData(reply_info, contents.data(), contents.length(), "text/javascript");
			}
			catch(FileUtils::FileUtilsExcep& e)
			{
				web::ResponseUtils::writeHTTPOKHeaderAndData(reply_info, e.what());
			}
		}
		else if(request.path == "/build/three.module.js")// TEMP HACK
		{
			try
			{
				std::string contents;
				FileUtils::readEntireFile("N:\\new_cyberspace\\trunk\\webclient\\build/three.module.js", contents);
				web::ResponseUtils::writeHTTPOKHeaderAndData(reply_info, contents.data(), contents.length(), "text/javascript");
			}
			catch(FileUtils::FileUtilsExcep& e)
			{
				web::ResponseUtils::writeHTTPOKHeaderAndData(reply_info, e.what());
			}
		}
#endif
		else
		{
			std::string page = "Unknown page";
			web::ResponseUtils::writeHTTPNotFoundHeaderAndData(reply_info, page);
		}
	}
}


bool WebServerRequestHandler::handleWebSocketConnection(Reference<SocketInterface>& socket)
{
	// Wrap socket in a websocket
	WebSocketRef websocket = new WebSocket(socket);

	// Handle the connection in a worker thread.
	Reference<WorkerThread> worker_thread = new WorkerThread(websocket, server);

	try
	{
		server->worker_thread_manager.addThread(worker_thread);
	}
	catch(MyThreadExcep& e)
	{
		// Will get this when thread creation fails.
		conPrint("ListenerThread failed to launch worker thread: " + e.what());
	}

	return true; // We do want to handle this connection completely.
}
