/*
 *
 *  Copyright (c) 2022
 *  name : Francis Banyikwa
 *  email: mhogomchungu@gmail.com
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "versionInfo.h"
#include "context.hpp"
#include "tabmanager.h"
#include "mainwindow.h"

versionInfo::versionInfo( Ui::MainWindow& ui,const Context& ctx ) :
	m_ui( ui ),
	m_ctx( ctx ),
	m_checkForEnginesUpdates( m_ctx.Settings().checkForEnginesUpdates() )
{
}

void versionInfo::log( const QString& msg,int id ) const
{
	m_ctx.logger().add( msg,id ) ;
}

void versionInfo::updateMediaDownloader( const engines::Iterator& iter ) const
{
	m_ctx.network().download( iter,{} ) ;
}

void versionInfo::done( versionInfo::extensionVersionInfo vInfo ) const
{
	QStringList m ;

	QStringList mm ;

	QString s = "%1: %2\n%3: %4\n%5: %6\n" ;

	auto nt = tr( "Engine Name" ) ;
	auto it = tr( "Installed Version" ) ;
	auto lt = tr( "Latest Version" ) ;

	vInfo.report( [ & ]( const QString& name,const QString& iv,const QString& lv ){

		mm.append( name ) ;
		m.append( s.arg( nt,name,it,iv,lt,lv ) ) ;
	} ) ;

	if( m.size() ){

		m_ctx.mainWindow().setTitle( tr( "There Is An Update For " ) + mm.join( ", " ) ) ;

		auto s = tr( "Update Found" ) ;

		m_ctx.logger().add( s + "\n" + m.join( "\n" ),utility::concurrentID() ) ;
	}
}

void versionInfo::done( printVinfo vinfo ) const
{
	if( vinfo.hasNext() ){

		this->check( vinfo.next() ) ;
	}else{
		vinfo.reportDone() ;

		if( vinfo.booting() ){

			this->checkForUpdates() ;
		}
	}
}

void versionInfo::checkForEnginesUpdates( versionInfo::extensionVersionInfo vInfo ) const
{
	const auto& engine = vInfo.engine() ;

	const auto& url = engine.downloadUrl() ;

	if( url.isEmpty() ){

		if( vInfo.hasNext() ){

			return this->checkForEnginesUpdates( vInfo.next() ) ;
		}else{
			return this->done( std::move( vInfo ) ) ;
		}
	}

	if( engine.name().contains( "yt-dlp" ) && engine.name() != "yt-dlp" ){

		if( vInfo.hasNext() ){

			return this->checkForEnginesUpdates( vInfo.next() ) ;
		}else{
			return this->done( std::move( vInfo ) ) ;
		}
	}

	m_ctx.network().get( url,[ this,vInfo = std::move( vInfo ) ]( const QByteArray& m ){

		auto lv = vInfo.engine().versionInfoFromGithub( m ) ;

		if( lv.version.valid() ){

			const auto& engine = vInfo.engine() ;

			engines::engine::exeArgs::cmd cmd( engine.exePath(),{ engine.versionArgument() } ) ;

			auto mm = QProcess::ProcessChannelMode::MergedChannels ;

			utils::qprocess::run( cmd.exe(),cmd.args(),mm,[ lv = std::move( lv ),this,vInfo = vInfo.move() ]( const utils::qprocess::outPut& r ){

				if( r.success() ){

					const auto& engine = vInfo.engine() ;

					util::version iv = engine.setVersionString( r.stdOut ) ;

					auto infov = vInfo.move() ;

					infov.append( engine.name(),std::move( iv ),std::move( lv.version ) ) ;

					if( infov.hasNext() ){

						return this->checkForEnginesUpdates( infov.next() ) ;
					}else{
						this->done( infov.move() ) ;
					}
				}else{
					if( vInfo.hasNext() ){

						return this->checkForEnginesUpdates( vInfo.next() ) ;
					}else{
						this->done( vInfo.move() ) ;
					}
				}
			} ) ;
		}else{
			if( vInfo.hasNext() ){

				return this->checkForEnginesUpdates( vInfo.next() ) ;
			}else{
				this->done( vInfo.move() ) ;
			}
		}
	} ) ;
}

void versionInfo::check( versionInfo::printVinfo vinfo ) const
{
	const auto& engine = vinfo.engine() ;

	auto m = vinfo.setAfterDownloading( false ) ;

	if( engine.usingPrivateBackend() && engine.validDownloadUrl() && networkAccess::hasNetworkSupport() ){

		if( engine.backendExists() ){

			if( m || vinfo.show() ){

				utility::setDefaultEngine( m_ctx,vinfo.defaultEngine() ) ;

				this->printEngineVersionInfo( std::move( vinfo ) ) ;
			}else{
				this->done( std::move( vinfo ) ) ;
			}
		}else{
			m_ctx.network().download( vinfo.iter(),vinfo.showVersionInfo(),vinfo.defaultEngine() ) ;
		}
	}else{
		if( engine.backendExists() ){

			if( vinfo.show() || m ){

				utility::setDefaultEngine( m_ctx,vinfo.defaultEngine() ) ;

				this->printEngineVersionInfo( std::move( vinfo ) ) ;
			}else{
				this->done( std::move( vinfo ) ) ;
			}
		}else{
			if( vinfo.show() ){

				auto m = QObject::tr( "Failed to find version information, make sure \"%1\" is installed and works properly" ).arg( engine.name() ) ;

				this->log( m,vinfo.iter().id() ) ;
			}else{
				this->done( std::move( vinfo ) ) ;
			}
		}
	}
}

void versionInfo::checkForUpdates() const
{
	if( m_ctx.Settings().checkForUpdates() ){

		auto url = "https://api.github.com/repos/mhogomchungu/media-downloader/releases/latest" ;

		m_ctx.network().get( url,[ this ]( const QByteArray& data ){

			if( !data.isEmpty() ){

				auto e = QJsonDocument::fromJson( data ) ;

				auto lv = e.object().value( "tag_name" ).toString() ;
				auto iv = utility::installedVersionOfMediaDownloader() ;

				versionInfo::extensionVersionInfo vInfo = m_ctx.Engines().getEnginesIterator() ;

				vInfo.append( "Media Downloader",iv,lv ) ;

				if( !m_ctx.Settings().checkForEnginesUpdates() ){

					this->checkForEnginesUpdates( std::move( vInfo ) ) ;
				}
			}
		} ) ;
	}
}

void versionInfo::printEngineVersionInfo( versionInfo::printVinfo vInfo ) const
{
	m_ctx.TabManager().disableAll() ;

	const auto& engine = vInfo.engine() ;

	auto id = utility::sequentialID() ;

	this->log( QObject::tr( "Checking installed version of" ) + " " + engine.name(),id ) ;

	if( engine.name().contains( "yt-dlp" ) && engine.name() != "yt-dlp" ){

		const auto& e = m_ctx.Engines().getEngineByName( "yt-dlp" ) ;

		if( e.has_value() ){

			const auto& version = e.value().versionInfo() ;

			if( version.valid() ){

				this->log( QObject::tr( "Found version" ) + ": " + version.toString(),id ) ;

				return this->done( vInfo ) ;
			}
		}
	}

	engines::engine::exeArgs::cmd cmd( engine.exePath(),{ engine.versionArgument() } ) ;

	if( !m_ctx.debug().isEmpty() ){

		auto exe = "cmd: \"" + cmd.exe() + "\"" ;

		for( const auto& it : cmd.args() ){

			exe += " \"" + it + "\"" ;
		}

		m_ctx.logger().add( exe,id ) ;
	}

	auto mm = QProcess::ProcessChannelMode::MergedChannels ;

	QFile qfile( cmd.exe() ) ;

	qfile.setPermissions( qfile.permissions() | QFileDevice::ExeOwner ) ;

	utils::qprocess::run( cmd.exe(),cmd.args(),mm,[ this,id,vInfo = std::move( vInfo ) ]( const utils::qprocess::outPut& r ){

		const auto& engine = vInfo.engine() ;

		if( r.success() ){

			this->log( QObject::tr( "Found version" ) + ": " + engine.setVersionString( r.stdOut ),id ) ;

			const auto& url = engine.downloadUrl() ;

			if( !url.isEmpty() && m_checkForEnginesUpdates ){

				m_ctx.network().get( url,[ id,this,vInfo = vInfo.move() ]( const QByteArray& m ){

					const auto& engine = vInfo.engine() ;

					const auto& versionOnline = engine.versionInfoFromGithub( m ) ;
					const auto& installedVersion = engine.versionInfo() ;

					const auto& version = versionOnline.version ;

					if( version.valid() && installedVersion.valid() && installedVersion < version ){

						auto m = versionOnline.stringVersion ;

						this->log( QObject::tr( "Newest Version Is %1, Updating" ).arg( m ) ,id ) ;

						m_ctx.network().download( vInfo.iter(),vInfo.showVersionInfo(),vInfo.defaultEngine() ) ;
					}else{
						m_ctx.TabManager().enableAll() ;

						this->done( std::move( vInfo ) ) ;
					}
				} ) ;

				return ;
			}else{
				m_ctx.TabManager().enableAll() ;
			}
		}else{
			this->log( QObject::tr( "Failed to find version information, make sure \"%1\" is installed and works properly" ).arg( engine.name() ),id ) ;

			m_ctx.TabManager().enableAll() ;

			engine.setBroken() ;
		}

		this->done( std::move( vInfo ) ) ;
	} ) ;
}

versionInfo::doneInterface::~doneInterface()
{
}
