#!/usr/bin/env python
"""BasePlayTracks - base class to test Playing of playlists of tracks.

Derived classes MUST

    - define self.tracks - the list of tracks to test (URI/Meta tuples)
    - define self.doc - docstring to print on parameter parse failure
    - call Test() method with following params
        - Sender DUT ['local' for internal SoftPlayer on loopback]
        - Receiver DUT ['local' for internal SoftPlayer on loopback] (None->not present)
        - Time to play before skipping to next (None = play all)
        - Repeat mode [on/off]
        - Shuffle mode [on/off]
        - (optional) dict of options to pass to sender SoftPlayer
"""

import _FunctionalTest
import BaseTest                   as BASE
import Upnp.ControlPoints.Volkano as Volkano
import _SoftPlayer                as SoftPlayer
import LogThread
import os
import random
import sys
import threading
import time


def Run( aArgs ):
    """Pass the Run() call up to the base class"""
    BASE.Run( aArgs )


class BasePlayTracks( BASE.BaseTest ):
    """Base class for XxxPlayTracks tests"""

    def __init__( self ):
        """Constructor - initialise base class"""
        BASE.BaseTest.__init__( self )
        self.doc              = 'Define docstring in derived class'
        self.sender           = None
        self.senderDev        = None
        self.softSender       = None
        self.receiver         = None
        self.receiverDev      = None
        self.softRcvr         = None
        self.playTimer        = None
        self.nextTimer        = None
        self.stateTimer       = None
        self.checkInfoTimer   = None
        self.lastIdTime       = time.time()
        self.tracks           = []
        self.repeat           = 'off'
        self.shuffle          = 'off'
        self.numTrack         = 0
        self.startTime        = 0
        self.playTime         = None
        self.senderPlayTime   = 0
        self.receiverPlayTime = 0
        self.expectedPlayTime = 0
        self.senderPlaying    = threading.Event()
        self.senderStarted    = threading.Event()
        self.senderStopped    = threading.Event()
        self.senderDuration   = threading.Event()
        self.trackChangeMutex = threading.Lock()
        random.seed()

    def Test( self, args ):
        """Play tracks, using Media services for control"""
        senderName   = ''
        receiverName = ''
        softOptions  = None
        loopback     = False

        try:
            senderName   = args[1]
            receiverName = args[2]
            if str( args[3] ).lower() != 'None':
                self.playTime = int( args[3] )
            if len( args ) > 4:
                self.repeat = args[4]
            if len( args ) > 5:
                self.shuffle = args[5]
            if len( args ) > 6:
                softOptions = args[6]
        except:
            print '\n', self.doc, '\n'
            self.log.Abort( '', 'Invalid arguments %s' % (str( args )) )

        if receiverName.lower() == 'none':
            receiverName = None
            
        if receiverName is not None:
            if receiverName.lower() == 'local' and senderName.lower() != 'local' or \
               senderName.lower() == 'local' and receiverName.lower() != 'local':
                self.log.Abort( '', 'Local loopback can only apply to ALL or NONE devices' )
        if senderName.lower() == 'local':
            loopback = True

        if self.repeat.lower() not in ('off', 'on'):
            self.log.Abort( '', 'Invalid repeat mode %s' % self.repeat )
            
        if self.shuffle.lower() not in ('off', 'on'):
            self.log.Abort( '', 'Invalid shuffle mode %s' % self.shuffle )
            
        # start local softplayer(s) as required
        if senderName.lower() == 'local':
            options = {'aRoom':'TestSender', 'aLoopback':loopback}
            if softOptions:
                options.update( softOptions )
            self.softSender = SoftPlayer.SoftPlayer( **options )
            senderName = self.softSender.name
        if receiverName is not None and receiverName.lower() == 'local':
            self.softRcvr = SoftPlayer.SoftPlayer( aRoom='TestRcvr', aLoopback=loopback )
            receiverName = self.softRcvr.name

        # create Sender device an put on random source (catch Volkano #2968, Network #894, #1807)
        self.senderDev = senderName.split( ':' )[0]
        self.sender = Volkano.VolkanoDevice( senderName, aIsDut=True, aLoopback=loopback )
        self.sender.product.sourceIndex = random.randint( 1, self.sender.product.sourceCount-1 )
        time.sleep( 3 )
        
        # create Receiver Device, put onto random source and connect to sender
        if receiverName:
            self.receiverDev = receiverName.split( ':' )[0]
            self.receiver = Volkano.VolkanoDevice( receiverName, aIsDut=True, aLoopback=loopback )
            self.receiver.product.sourceIndex = random.randint( 0, self.receiver.product.sourceCount-1 )
            time.sleep( 3 )
            self.receiver.receiver.SetSender( self.sender.sender.uri, self.sender.sender.metadata )
            self.receiver.receiver.Play()
        
        # add and verify playlist and set playback mode
        self.senderStopped.clear()        
        self.sender.playlist.DeleteAllTracks()
        self.sender.playlist.repeat = self.repeat
        self.sender.playlist.shuffle = self.shuffle
        self.sender.playlist.AddPlaylist( self.tracks )
        time.sleep( 3 )
        
        # check the playlist ReadList operation
        self._CheckReadList()

        # subscribe to sender and receiver events
        self.sender.playlist.AddSubscriber( self._SenderPlaylistCb )
        self.sender.time.AddSubscriber( self._SenderTimeCb )
        if receiverName:
            self.receiver.time.AddSubscriber( self._ReceiverTimeCb )
            self.receiver.receiver.AddSubscriber( self._ReceiverReceiverCb )

        # start playback
        self.log.Info( self.senderDev, 'Starting on source %s' % self.sender.product.sourceIndex )
        self.sender.playlist.SeekIndex( 0 )
        self.senderPlaying.wait( 10 )
        if not self.senderStarted.is_set():
            self.log.Fail( self.senderDev, 'Playback never started' )
        else:
            self._TrackChanged( self.sender.playlist.id, self.sender.playlist.id )
            self.senderStopped.clear()
            self.senderStopped.wait()
                
    def Cleanup( self ):
        """Perform post-test cleanup"""
        if self.checkInfoTimer:
            self.checkInfoTimer.cancel()
        if self.playTimer:
             self.playTimer.cancel()
        if self.nextTimer:
            self.nextTimer.cancel()
        if self.stateTimer:
            self.stateTimer.cancel()
        if self.sender:
            self.sender.Shutdown()
        if self.receiver: 
            self.receiver.Shutdown() 
        if self.softSender:
            self.softSender.Shutdown()
        if self.softRcvr:
            self.softRcvr.Shutdown()
        BASE.BaseTest.Cleanup( self )

    # noinspection PyUnusedLocal
    def _SenderTimeCb( self, service, svName, svVal, svSeq ):
        """Callback from Time Service UPnP events on sender device"""
        if svName == 'Seconds':
            if svVal not in [0,'0']:
                # Ignore 0 reading to work around race at track change giving
                # false (=0) results for play time as this event for next track
                # has already occurred
                self.senderPlayTime = int( svVal )
        elif svName == 'Duration':
            if svVal != 0:
                self.senderDuration.set()

    # noinspection PyUnusedLocal
    def _SenderPlaylistCb( self, service, svName, svVal, svSeq ):
        """Callback from Playlist Service UPnP events on sender device"""
        if svName == 'TransportState':
            self.senderPlaying.clear()
            if self.stateTimer:   # add hysteresis to transport state comparison
                self.stateTimer.cancel()
                self.stateTimer = LogThread.Timer( 3, self._ReceiverStateCb )
                self.stateTimer.start()
            if svVal == 'Stopped':
                self.senderStopped.set()
            elif svVal == 'Playing':
                self.senderPlaying.set()
                self.senderStarted.set()
        elif svName == 'Id':
            if self.nextTimer:
                # cancel timer which is required to trigger the TrackChanged
                # code if NO Id event is received (same track plays 2x in a row)
                self.nextTimer.cancel()
                self.nextTimer = None
            # Run on seperate thread so Playback events not blocked
            if svVal != 0:
                thread = LogThread.Thread( target=self._TrackChanged, args=[int(svVal),self.sender.playlist.id] )
                thread.start()

    def _TrackChanged( self, aId, aPlId ):
        """Track changed - check results and setup timer for next track"""
        checkResults = True
        currIdTime = time.time()
        self.numTrack += 1

        if currIdTime-self.lastIdTime < 2:
            # less than 2s between ID events - assume previous track skipped
            self.log.Fail( self.senderDev, 'Track %d did NOT play' % (self.numTrack-1) )
            checkResults = False
            if not self.senderStopped.isSet():
                plIndex = self.sender.playlist.PlaylistIndex( aPlId )
                self.log.Header1( '', 'Track %d (Playlist #%d) Rpt->%s Shfl->%s' % \
                    (self.numTrack, plIndex+1, self.repeat, self.shuffle) )
        self.lastIdTime = currIdTime

        if aId>0 and checkResults:
            # Previous track played -> check its results
            self.trackChangeMutex.acquire()
            if self.checkInfoTimer:
                self.checkInfoTimer.cancel()
            self._CheckPlayTime()
            if not self.senderStopped.isSet():
                if self.repeat=='off' and self.numTrack>len( self.tracks ):
                    self.senderStopped.wait( 3 )
                    if not self.senderStopped.isSet():
                        self.log.Fail( self.senderDev, 'No stop on end-of-playlist' )
                        self.senderStopped.set()    # force test exit

            if not self.senderStopped.isSet():
                plIndex = self.sender.playlist.PlaylistIndex( aPlId )
                self.log.Header1( '', 'Track %d (Playlist #%d) Rpt->%s Shfl->%s' % \
                    (self.numTrack, plIndex+1, self.repeat, self.shuffle) )
                self._SetupPlayTimer()
                self.checkInfoTimer = LogThread.Timer( 3, self._CheckInfo, args=[aId] )
                self.checkInfoTimer.start()
            self.trackChangeMutex.release()

    def _CheckInfo( self, aId ):
        """Check sender and receiver info is as expected"""
        self.checkInfoTimer = None
        self._CheckSenderInfo( aId )
        if self.receiver:
            self._CheckReceiverInfo()

    def _CheckSenderInfo( self, aId ):
        """Check sender info is as expected"""
        if self.senderStarted.isSet():
            dsTrack = self.sender.playlist.TrackInfo( aId )
            if dsTrack:
                (uri,meta) = self.tracks[self.sender.playlist.idArray.index( aId )]
                if dsTrack['Uri'] != uri:
                    self.log.Fail( self.senderDev, 'Sender URI mismatch %s / %s'
                                    % (dsTrack['aUri'], uri) )
                else:
                    self.log.Pass( self.senderDev, 'Sender URI as expected' )

                if dsTrack['Metadata'] != meta:
                    self.log.Fail( self.senderDev, 'Sender metadata mismatch %s / %s'
                                    % (dsTrack['Metadata'], meta) )
                else:
                    self.log.Pass( self.senderDev, 'Sender metadata as expected' )

                self.log.FailUnless( self.senderDev, self.sender.sender.audio,
                    'Sender Audio flag is %s' % self.sender.sender.audio )
                self.log.FailUnless( self.senderDev, self.sender.sender.status == 'Enabled',
                    'Sender Status is %s' % self.sender.sender.status )
            else:
                self.log.Fail( self.senderDev, 'No track data returned' )
                self.senderStopped.set()     # force test exit

    def _CheckReceiverInfo( self ):
        """Check receiver Info matches sender"""
        # called 3s after track change to allow events from both sides to be
        # updated to current values - test skipped if track play time < 10s
        for item in ['bitDepth', 'bitRate', 'codecName', 'duration', 'lossless',
                     'metadata', 'metatext', 'sampleRate', 'uri']:
            itemTitle = item[0].upper()+item[1:]
            senderVal = eval( 'self.sender.info.polled%s' % itemTitle )
            receiverVal = eval( 'self.receiver.info.polled%s' % itemTitle )
            self.log.FailUnless( self.receiverDev, senderVal==receiverVal,
                '(%s/%s) POLLED Sender/Receiver value for %s' %
                (str( senderVal ), str( receiverVal ), item[0].upper()+item[1:] ))

            senderVal = eval( 'self.sender.info.%s' % item )
            receiverVal = eval( 'self.receiver.info.%s' % item )
            self.log.FailUnless( self.receiverDev, senderVal==receiverVal,
                '(%s/%s) EVENTED Sender/Receiver value for %s' %
                (str( senderVal ), str( receiverVal ), itemTitle) )

    # noinspection PyUnusedLocal
    def _ReceiverTimeCb( self, service, svName, svVal, svSeq ):
        """Callback from Time Service UPnP events on receiver device"""
        if svName == 'Seconds':
            if svVal not in [0,'0']:
                # Ignore 0 reading to work around race at track change giving
                # false (=0) results for play time as this event for next track
                # has already occurred
                self.receiverPlayTime = int( svVal )

    # noinspection PyUnusedLocal
    def _ReceiverReceiverCb( self, service, svName, svVal, svSeq ):
        """Callback from Receiver Service UPnP events on receiver device"""
        if svName == 'TransportState':
            # add hysteresis to transport state comparison
            if self.stateTimer:
                self.stateTimer.cancel()
            self.stateTimer = LogThread.Timer( 3, self._ReceiverStateCb )
            self.stateTimer.start()
            
    def _ReceiverStateCb( self ):
        """Timer callback from receiver transport state event"""
        senderState = self.sender.playlist.transportState
        receiverState = self.receiver.receiver.transportState
        if receiverState == 'Playing':
            self.log.FailUnless( self.receiverDev, senderState=='Playing',
                '(%s/%s) sender/receiver Transport State' % (senderState, receiverState) )
        elif receiverState == 'Waiting':  
            self.log.FailIf( self.receiverDev, senderState=='Playing',
                '(%s/%s) sender/receiver Transport State' % (senderState, receiverState) )
                
    def _SetupPlayTimer( self ):
        """Setup timer to callback after specified play time"""
        start = time.time()
        time.sleep( 0.2 )   # sometimes events ID before buffering state
        self.senderStarted.wait()
        self.senderPlaying.wait( 10 )
        delay = time.time()-start
        if delay > 1:
            self.log.Warn( self.senderDev, 'Slow startup of track playback (%.2fs)' % delay )
        self.startTime = time.time()
        self.senderDuration.clear()
        self.expectedPlayTime = self.sender.time.duration
        if self.expectedPlayTime == 0:
            self.senderDuration.wait( 3 )
            if self.senderDuration.is_set():
                self.expectedPlayTime = self.sender.time.duration
            else:
                self.log.Fail( self.senderDev, 'No evented duration for track %d' % self.numTrack )
        if self.playTime is not None and self.playTime < self.expectedPlayTime:
            self.expectedPlayTime = self.playTime
            if self.playTime == 0:
                self._PlayTimerCb()
            else:
                if self.playTimer:
                    self.playTimer.cancel()
                self.playTimer = LogThread.Timer(
                    self.playTime-(time.time()-self.startTime), self._PlayTimerCb )
                self.playTimer.start()

    def _PlayTimerCb( self ):
        """Callback from playtime timer - skips to next track"""
        self.sender.playlist.Next()
        # The 'next' timer is needed to stimulate track change code when there
        # has been no track change (same track played twice in a row). It will
        # be cancelled before expiry if an Id event (new track) is received
        # from the Playlist service 
        if not self.nextTimer:
            self.nextTimer = LogThread.Timer( 1, self._NextTimerCb )
            self.nextTimer.start()
        
    def _NextTimerCb( self ):
        """Next Timer CB - called on expiry of the 'NextTimer'"""
        # Next timer will call into this 1s after a track change has been
        # forced by the 'Next' action (unless the timer has been cancelled due
        # to receipt of an Id event from the Playlist service). This is to 
        # handle cases where the same track is played twice in a row, which
        # causes NO id event to be sent.
        self.nextTimer = None
        self.log.Info( self.senderDev, 'Next track timer triggered track change as no Id event Rx' ) 
        self._TrackChanged( self.sender.playlist.id, self.sender.playlist.id )

    def _CheckPlayTime( self ):
        """Verify track played for expected duration (see Trac #1527)"""
        if self.expectedPlayTime:
            loLim = self.expectedPlayTime-1
            hiLim = self.expectedPlayTime+1
            self.log.CheckLimits( self.senderDev, 'GELE', self.senderPlayTime, loLim, hiLim,
                'reported (by Sender) play time')
            if self.receiver:
                self.log.CheckLimits( self.receiverDev, 'GELE', self.receiverPlayTime, loLim, hiLim,
                    'reported (by Receiver) play time')
            self.log.CheckLimits( '', 'GELE', int( round( time.time()-self.startTime, 0 )),
                loLim, hiLim, 'measured (by test) play time')
            
    def _CheckReadList( self ):
        """Check operation of the Playlist ReadList action"""
        # Test ReadList using a random population sample from the entire current
        # IDArray, and randomly append a (probably) invalid ID to this test list
        fail        = False
        totalTracks = len( self.sender.playlist.idArray )
        numTracks   = random.randint( 0, totalTracks )
        ids         = random.sample( self.sender.playlist.idArray, numTracks )
        if random.randint( 0, 1 ):
            numTracks += 1
            totalTracks += 1
            ids.append( 9876543 )

        self.log.Info( self.senderDev, 'Checking ReadList on %d random tracks from %d' %
            (numTracks, totalTracks) )

        readTracks = self.sender.playlist.TracksInfo( ids )
        for tId in readTracks.keys():
            readUri  = readTracks[tId][0]
            readMeta = readTracks[tId][1]
            try:
                (expUri, expMeta)  = self.tracks[self.sender.playlist.idArray.index( tId )]
            except:
                (expUri, expMeta)  = ('', '')
            expMeta = expMeta.decode( 'utf-8' )
            if readUri is None: readUri = ''
            if readMeta is None: readMeta = ''

            if readUri != expUri:
                fail = True
                self.log.Fail( self.senderDev, 'Actual/Expected URI read from DS %s | %s' % (readUri, expUri) )
            if readMeta != expMeta:
                fail = True
                self.log.Fail( self.senderDev, 'Actual/Expected META read from DS %s | %s' % (readMeta, expMeta) )
        if not fail:
            self.log.Pass( self.senderDev, 'All Playlist ReadList tracks OK' )