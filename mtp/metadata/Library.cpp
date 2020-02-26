#include <mtp/metadata/Library.h>
#include <mtp/ptp/Session.h>
#include <mtp/log.h>

namespace mtp
{
	namespace
	{
		ByteArray ArtistUUID 	= { 0x00, 0x08, 0x8f, 0xa7, 0x00, 0x06, 0xdb, 0x11, 0x89, 0xca, 0x00, 0x19, 0xb9, 0x2a, 0x39, 0x33 };
		//ByteArray AlbumUUID 	= { 0x00, 0xb6, 0xe8, 0x33, 0x00, 0x01, 0xdb, 0x11, 0x89, 0xca, 0x00, 0x19, 0xb9, 0x2a, 0x39, 0x33 };
		ByteArray AlbumUUID 	= { 0x06, 0x27, 0x6d, 0x0d, 0x00, 0x01, 0xdb, 0x11, 0x89, 0xca, 0x00, 0x19, 0xb9, 0x2a, 0x39, 0x33 };
	}

	Library::Library(const mtp::SessionPtr & session): _session(session)
	{
		msg::ObjectHandles rootFolders = _session->GetObjectHandles(Session::AllStorages, mtp::ObjectFormat::Association, Session::Root);
		for (auto id : rootFolders.ObjectHandles) {
			auto name = _session->GetObjectStringProperty(id, ObjectProperty::ObjectFilename);
			if (name == "Artists")
				_artistsFolder = id;
			else if (name == "Albums")
				_albumsFolder = id;
		}

		if (_artistsFolder == ObjectId() || _albumsFolder == ObjectId())
			throw std::runtime_error("fixme: restore standard folder structure");

		debug("artists folder: ", _artistsFolder.Id);
		debug("albums folder: ", _albumsFolder.Id);

		using namespace mtp;
		{
			auto artists = _session->GetObjectHandles(Session::AllStorages, ObjectFormat::Artist, Session::Device);
			for (auto id : artists.ObjectHandles)
			{
				auto name = _session->GetObjectStringProperty(id, ObjectProperty::Name);
				debug("artist: ", name, "\t", id.Id);
				auto artist = std::make_shared<Artist>();
				artist->Id = id;
				artist->Name = name;
				_artists.insert(std::make_pair(name, artist));
			}
		}
		{
			auto albums = _session->GetObjectHandles(Session::AllStorages, ObjectFormat::AudioAlbum, Session::Device);
			for (auto id : albums.ObjectHandles)
			{
				auto name = _session->GetObjectStringProperty(id, ObjectProperty::Name);
				auto artistName = _session->GetObjectStringProperty(id, ObjectProperty::Artist);
				auto artist = GetArtist(artistName);
				if (!artist)
					error("invalid artist name in album ", name);

				debug("album: ", name, "\t", id.Id);
				auto album = std::make_shared<Album>();
				album->Name = name;
				album->Id = id;
				album->Year = 0;
				_albums.insert(std::make_pair(std::make_pair(artist, name), album));
			}
		}
	}

	Library::~Library()
	{ }

	Library::ArtistPtr Library::CreateArtist(const std::string & name)
	{
		if (name.empty())
			return nullptr;

		ByteArray propList;
		OutputStream os(propList);

		os.Write32(3); //number of props

		os.Write32(0); //object handle
		os.Write16(static_cast<u16>(ObjectProperty::Name));
		os.Write16(static_cast<u16>(DataTypeCode::String));
		os.WriteString(name);

		os.Write32(0); //object handle
		os.Write16(static_cast<u16>(ObjectProperty::ContentTypeUUID));
		os.Write16(static_cast<u16>(DataTypeCode::Uint128));
		os.WriteData(ArtistUUID);

		os.Write32(0); //object handle
		os.Write16(static_cast<u16>(ObjectProperty::ObjectFilename));
		os.Write16(static_cast<u16>(DataTypeCode::String));
		os.WriteString(name + ".art");

		auto response = _session->SendObjectPropList(Session::AnyStorage, Session::Device, ObjectFormat::Artist, 0, propList);
		auto artist = std::make_shared<Artist>();
		artist->Id = response.ObjectId;
		artist->Name = name;
		_artists.insert(std::make_pair(name, artist));
		return artist;
	}

	Library::AlbumPtr Library::CreateAlbum(const ArtistPtr & artist, const std::string & name)
	{
		if (name.empty() || !artist)
			return nullptr;

		ByteArray propList;
		OutputStream os(propList);

		os.Write32(5); //number of props

		os.Write32(0); //object handle
		os.Write16(static_cast<u16>(ObjectProperty::ParentObject));
		os.Write16(static_cast<u16>(DataTypeCode::Uint32));
		os.Write32(_artistsFolder.Id);

		os.Write32(0); //object handle
		os.Write16(static_cast<u16>(ObjectProperty::ArtistId));
		os.Write16(static_cast<u16>(DataTypeCode::Uint32));
		os.Write32(artist->Id.Id);

		os.Write32(0); //object handle
		os.Write16(static_cast<u16>(ObjectProperty::Name));
		os.Write16(static_cast<u16>(DataTypeCode::String));
		os.WriteString(name);

		os.Write32(0); //object handle
		os.Write16(static_cast<u16>(ObjectProperty::ContentTypeUUID));
		os.Write16(static_cast<u16>(DataTypeCode::Uint128));
		os.WriteData(AlbumUUID);

		os.Write32(0); //object handle
		os.Write16(static_cast<u16>(ObjectProperty::ObjectFilename));
		os.Write16(static_cast<u16>(DataTypeCode::String));
		os.WriteString(artist->Name + "--" + name + ".alb");

		auto response = _session->SendObjectPropList(Session::AnyStorage, Session::Device, ObjectFormat::AudioAlbum, 0, propList);

		auto album = std::make_shared<Album>();
		album->Id = response.ObjectId;
		album->Artist = artist;
		album->Name = name;
		_albums.insert(std::make_pair(std::make_pair(artist, name), album));
		return album;
	}

	void Library::CreateTrack(ArtistPtr artist, AlbumPtr album, ObjectFormat type, const std::string &name, const std::string &filename, size_t size)
	{
		ByteArray propList;
		OutputStream os(propList);

		os.Write32(6); //number of props

		os.Write32(0); //object handle
		os.Write16(static_cast<u16>(ObjectProperty::ArtistId));
		os.Write16(static_cast<u16>(DataTypeCode::Uint32));
		os.Write32(artist->Id.Id);

		os.Write32(0); //object handle
		os.Write16(static_cast<u16>(ObjectProperty::Artist));
		os.Write16(static_cast<u16>(DataTypeCode::String));
		os.WriteString(artist->Name);

		os.Write32(0); //object handle
		os.Write16(static_cast<u16>(ObjectProperty::AlbumId));
		os.Write16(static_cast<u16>(DataTypeCode::Uint32));
		os.Write32(album->Id.Id);

		os.Write32(0); //object handle
		os.Write16(static_cast<u16>(ObjectProperty::AlbumName));
		os.Write16(static_cast<u16>(DataTypeCode::String));
		os.WriteString(album->Name);

		os.Write32(0); //object handle
		os.Write16(static_cast<u16>(ObjectProperty::Name));
		os.Write16(static_cast<u16>(DataTypeCode::String));
		os.WriteString(name);

		os.Write32(0); //object handle
		os.Write16(static_cast<u16>(ObjectProperty::ObjectFilename));
		os.Write16(static_cast<u16>(DataTypeCode::String));
		os.WriteString(filename);

		_session->SendObjectPropList(Session::AnyStorage, Session::Device, type, size, propList);
	}

}