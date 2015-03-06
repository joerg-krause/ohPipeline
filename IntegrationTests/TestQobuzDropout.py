#!/usr/bin/env python
"""TestQobuzDropout - test for dropout from Qobuz served tracks.

Parameters:
    arg#1 - Sender DUT
    arg#2 - Receiver/Repeater DUT (None = not present)
    arg#3 - Receiver/SlaveDUT  (None = not present)
    arg#4 - Test duration (secs) or 'forever'
    arg#5 - Number of tracks to test with (use 0 for fixed list of 20 hard-coded tracks)
    arg#6 - Qobuz ID
    arg#7 - Qobuz username
    arg#8 - Qobuz password
local' for internal SoftPlayer on loopback for DUTs

Verifies Qobuz served audio played by the DUT does not suffer from audio dropout.
Additionally checks for dropout on downstream songcast receiver(s)
"""

import _FunctionalTest
import _BaseDropout    as BASE
import os
import sys
import time
import threading

kQobuzCreds  = 'qobuz.com'
kQobuzTracks = [
    ('qobuz://track?version=2&trackId=2126599',  '<DIDL-Lite xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/"><item><dc:title xmlns:dc="http://purl.org/dc/elements/1.1/">Just Kissed My Baby (Album Version)</dc:title><upnp:class xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">object.item.audioItem.musicTrack</upnp:class><upnp:album xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Rejuvenation (US Release)</upnp:album><upnp:artist xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">The Meters</upnp:artist><res>qobuz://track?version=2&amp;trackId=2126599</res></item></DIDL-Lite>'),
    ('qobuz://track?version=2&trackId=6374329',  '<DIDL-Lite xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/"><item><dc:title xmlns:dc="http://purl.org/dc/elements/1.1/">Gangnam Style (\xea\xb0\x95\xeb\x82\xa8\xec\x8a\xa4\xed\x83\x80\xec\x9d\xbc)</dc:title><upnp:class xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">object.item.audioItem.musicTrack</upnp:class><upnp:album xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Gangnam Style (\xea\xb0\x95\xeb\x82\xa8\xec\x8a\xa4\xed\x83\x80\xec\x9d\xbc)</upnp:album><upnp:artist xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">PSY</upnp:artist><res>qobuz://track?version=2&amp;trackId=6374329</res></item></DIDL-Lite>'),
    ('qobuz://track?version=2&trackId=2409705',  '<DIDL-Lite xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/"><item><dc:title xmlns:dc="http://purl.org/dc/elements/1.1/">Princess Of The Dawn</dc:title><upnp:class xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">object.item.audioItem.musicTrack</upnp:class><upnp:album xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Best Of Accept</upnp:album><upnp:artist xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Accept</upnp:artist><res>qobuz://track?version=2&amp;trackId=2409705</res></item></DIDL-Lite>'),
    ('qobuz://track?version=2&trackId=5032878',  '<DIDL-Lite xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/"><item><dc:title xmlns:dc="http://purl.org/dc/elements/1.1/">Always the Same</dc:title><upnp:class xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">object.item.audioItem.musicTrack</upnp:class><upnp:album xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Nothing For Granted</upnp:album><upnp:artist xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Sandra Nkak\xc3\xa9</upnp:artist><res>qobuz://track?version=2&amp;trackId=5032878</res></item></DIDL-Lite>'),
    ('qobuz://track?version=2&trackId=19027802', '<DIDL-Lite xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/"><item><dc:title xmlns:dc="http://purl.org/dc/elements/1.1/">Didone abbandonata : Act III. Scene XIII : Gi\xc3\xa0 si desta la tempesta (Araspe)</dc:title><upnp:class xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">object.item.audioItem.musicTrack</upnp:class><upnp:album xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Il maestro Porpora : Arias</upnp:album><upnp:artist xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Franco Fagioli</upnp:artist><res>qobuz://track?version=2&amp;trackId=19027802</res></item></DIDL-Lite>'),
    ('qobuz://track?version=2&trackId=4407756',  '<DIDL-Lite xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/"><item><dc:title xmlns:dc="http://purl.org/dc/elements/1.1/">Wish You Were Here (2011 Remastered Version)</dc:title><upnp:classxmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">object.item.audioItem.musicTrack</upnp:class><upnp:album xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">A Foot in the Door: The Best Of Pink Floyd (2011 - Remaster)</upnp:album><upnp:artist xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Pink Floyd</upnp:artist><res>qobuz://track?version=2&amp;trackId=4407756</res></item></DIDL-Lite>'),
    ('qobuz://track?version=2&trackId=754280',   '<DIDL-Lite xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/"><item><dc:title xmlns:dc="http://purl.org/dc/elements/1.1/">Speaking With The Angel</dc:title><upnp:class xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">object.item.audioItem.musicTrack</upnp:class><upnp:album xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Ron Sexsmith</upnp:album><upnp:artist xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Ron Sexsmith</upnp:artist><res>qobuz://track?version=2&amp;trackId=754280</res></item></DIDL-Lite>'),
    ('qobuz://track?version=2&trackId=1946325',  '<DIDL-Lite xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/"><item><dc:title xmlns:dc="http://purl.org/dc/elements/1.1/">Beautiful (feat. Pharrell; Uncle Charlie Wilson)</dc:title><upnp:class xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">object.item.audioItem.musicTrack</upnp:class><upnp:album xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">The Best Of Snoop Dogg</upnp:album><upnp:artist xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Snoop Dogg</upnp:artist><res>qobuz://track?version=2&amp;trackId=1946325</res></item></DIDL-Lite>'),
    ('qobuz://track?version=2&trackId=5608216',  '<DIDL-Lite xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/"><item><dc:title xmlns:dc="http://purl.org/dc/elements/1.1/">Land of...</dc:title><upnp:class xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">object.item.audioItem.musicTrack</upnp:class><upnp:album xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Tourist (Remastered)</upnp:album><upnp:artist xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">St Germain</upnp:artist><res>qobuz://track?version=2&amp;trackId=5608216</res></item></DIDL-Lite>'),
    ('qobuz://track?version=2&trackId=14048788', '<DIDL-Lite xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/"><item><dc:title xmlns:dc="http://purl.org/dc/elements/1.1/">Come Away With Me</dc:title><upnp:class xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">object.item.audioItem.musicTrack</upnp:class><upnp:album xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Come Away With Me</upnp:album><upnp:artist xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Norah Jones</upnp:artist><res>qobuz://track?version=2&amp;trackId=14048788</res></item></DIDL-Lite>'),
    ('qobuz://track?version=2&trackId=5245431',  '<DIDL-Lite xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/"><item><dc:title xmlns:dc="http://purl.org/dc/elements/1.1/">(Guitar)</dc:title><upnp:class xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">object.item.audioItem.musicTrack</upnp:class><upnp:album xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">An Awesome Wave</upnp:album><upnp:artist xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Alt-J</upnp:artist><res>qobuz://track?version=2&amp;trackId=5245431</res></item></DIDL-Lite>'),
    ('qobuz://track?version=2&trackId=18003720', '<DIDL-Lite xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/"><item><dc:title xmlns:dc="http://purl.org/dc/elements/1.1/">The Gypsy (Live)</dc:title><upnp:class xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">object.item.audioItem.musicTrack</upnp:class><upnp:album xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Live at the Olympia - June 27; 2012</upnp:album><upnp:artist xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Ahmad Jamal</upnp:artist><res>qobuz://track?version=2&amp;trackId=18003720</res></item></DIDL-Lite>'),
    ('qobuz://track?version=2&trackId=10279840', '<DIDL-Lite xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/"><item><dc:title xmlns:dc="http://purl.org/dc/elements/1.1/">Soul Storm Comin\'</dc:title><upnp:class xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">object.item.audioItem.musicTrack</upnp:class><upnp:album xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Smoke and Noise</upnp:album><upnp:artist xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Chris Jones</upnp:artist><res>qobuz://track?version=2&amp;trackId=10279840</res></item></DIDL-Lite>'),
    ('qobuz://track?version=2&trackId=15215474', '<DIDL-Lite xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/"><item><dc:title xmlns:dc="http://purl.org/dc/elements/1.1/">Out Among the Stars</dc:title><upnp:class xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">object.item.audioItem.musicTrack</upnp:class><upnp:album xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Out Among The Stars</upnp:album><upnp:artist xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Johnny Cash</upnp:artist><res>qobuz://track?version=2&amp;trackId=15215474</res></item></DIDL-Lite>'),
    ('qobuz://track?version=2&trackId=7981738',  '<DIDL-Lite xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/"><item><dc:title xmlns:dc="http://purl.org/dc/elements/1.1/">Freak Show</dc:title><upnp:class xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">object.item.audioItem.musicTrack</upnp:class><upnp:album xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Trax 5 - Songs For The Next Generation</upnp:album><upnp:artist xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Interpr\xc3\xa8tes Divers</upnp:artist><res>qobuz://track?version=2&amp;trackId=7981738</res></item></DIDL-Lite>'),
    ('qobuz://track?version=2&trackId=1404606',  '<DIDL-Lite xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/"><item><dc:title xmlns:dc="http://purl.org/dc/elements/1.1/">G\xc3\xa9n\xc3\xa9rique de fin (Jour de f\xc3\xaate)</dc:title><upnp:class xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">object.item.audioItem.musicTrack</upnp:class><upnp:album xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Sonorama !</upnp:album><upnp:artist xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Jacques Tati</upnp:artist><res>qobuz://track?version=2&amp;trackId=1404606</res></item></DIDL-Lite>'),
    ('qobuz://track?version=2&trackId=14990559', '<DIDL-Lite xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/"><item><dc:title xmlns:dc="http://purl.org/dc/elements/1.1/">Concerto pour violoncelle en La Mineur; Wq 170 : II. Andante</dc:title><upnp:class xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">object.item.audioItem.musicTrack</upnp:class><upnp:album xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Carl Philipp Emanuel Bach</upnp:album><upnp:artist xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Oph\xc3\xa9lie Gaillard</upnp:artist><res>qobuz://track?version=2&amp;trackId=14990559</res></item></DIDL-Lite>'),
    ('qobuz://track?version=2&trackId=4283691',  '<DIDL-Lite xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/"><item><dc:title xmlns:dc="http://purl.org/dc/elements/1.1/">Wait</dc:title><upnp:class xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">object.item.audioItem.musicTrack</upnp:class><upnp:album xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Hurry Up; We\'re Dreaming.</upnp:album><upnp:artist xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">M83</upnp:artist><res>qobuz://track?version=2&amp;trackId=4283691</res></item></DIDL-Lite>'),
    ('qobuz://track?version=2&trackId=14823763', '<DIDL-Lite xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/"><item><dc:title xmlns:dc="http://purl.org/dc/elements/1.1/">The Bells; Op. 35: I. Allegro ma non tanto (\'The Silver Sleigh Bells\')</dc:title><upnp:class xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">object.item.audioItem.musicTrack</upnp:class><upnp:album xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Rachmaninov: Symphonic Dances; The Bells (\xc3\x89dition StudioMasters)</upnp:album><upnp:artist xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Sir Simon Rattle</upnp:artist><res>qobuz://track?version=2&amp;trackId=14823763</res></item></DIDL-Lite>'),
    ('qobuz://track?version=2&trackId=8148950',  '<DIDL-Lite xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/"><item><dc:title xmlns:dc="http://purl.org/dc/elements/1.1/">Kiko (Live)</dc:title><upnp:class xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">object.item.audioItem.musicTrack</upnp:class><upnp:album xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">In Concert</upnp:album><upnp:artist xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">Dead Can Dance</upnp:artist><res>qobuz://track?version=2&amp;trackId=8148950</res></item></DIDL-Lite>')]


class TestQobuzDropout( BASE.BaseDropout ):
    """Test dropout on of Qobuz served tracks"""

    def __init__( self ):
        """Constructor - initialise base class"""
        BASE.BaseDropout.__init__( self )
        self.doc = __doc__
        self.qobuzId   = ''
        self.qobuzUser = ''
        self.qobuzPwd  = ''
        self.seqNum    = threading.Event()

    def Test( self, args ):
        """Test dropout on of TIDAL served tracks"""
        numTracks = 0
        try:
            numTracks    = int( args[5] )
            self.qobuzId   = args[6]
            self.qobuzUser = args[7]
            self.qobuzPwd  = args[8]
            args[5] = {'aQobuzId':args[6]}
        except:
            print '\n', __doc__, '\n'
            self.log.Abort( '', 'Invalid arguments %s' % (str( args )) )

        if numTracks:
            import Instruments.Network.Qobuz as Qobuz
            qobuz = Qobuz.Qobuz( self.qobuzId )
            self.tracks = qobuz.RandomTracks( numTracks )
            self.log.Pass( '', 'Testing with list of %d randomly selected' % numTracks )
        else:
            self.tracks = kQobuzTracks
            self.log.Pass( '', 'Testing with list of 20 hard-coded tracks')
        BASE.BaseDropout.Test( self, args )

    def Cleanup( self ):
        """Perform post-test cleanup, logout of TIDAL"""
        if self.sender:
            self.sender.credentials.Clear( kQobuzCreds )
        BASE.BaseDropout.Cleanup( self )

    def SenderSetup( self ):
        """Login to and enable TIDAL"""
        self.sender.credentials.AddSubscriber( self._CredentialsCb )
        self.seqNum.clear()
        self.sender.credentials.Set( kQobuzCreds, self.qobuzUser, self.qobuzPwd )
        self.seqNum.wait( 10 )
        time.sleep( 1 )
        err = self.sender.credentials.Status( kQobuzCreds )
        if err:
            self.log.Abort( 'Qobuz', 'Login error: %s' % err )
        else:
            self.sender.credentials.SetEnabled( kQobuzCreds, True )
            self.log.Pass( self.senderDev, 'Logged into Qobuz' )
        self.sender.credentials.RemoveSubscriber( self._CredentialsCb )

    # noinspection PyUnusedLocal
    def _CredentialsCb( self, aService, aSvName, aSvVal, aSvSeq ):
        """Callback from Sender's Credentials service events"""
        if aSvName == 'SequenceNumber':
            self.seqNum.set()


if __name__ == '__main__':
    
    BASE.Run( sys.argv )
        