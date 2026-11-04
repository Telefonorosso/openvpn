/* AmiOpenVPN / AmiNetXDuo POC1: reversible IPv4 default route.
 * For AmiNetXDuo nightly 2026-11-03-c481528d or newer.
 * No assumption that AmigaDOS expands ${variable}, and no dependency on
 * OpenVPN route_vpn_gateway (absent in the tested port).
 * The VPN peer is read from DEVS:NetInterfaces/ptp0.
 * trusted_ip is captured by RouteUp.script with Shell GET, not C:Set.
 * WARNING: this script does not alter DNS or install a kill switch.
 */
OPTIONS RESULTS
ADDRESS COMMAND
PARSE ARG action
state  = 'RAM:AOVPN-XD-Routes.state'
status = 'RAM:AOVPN-XD-Routes.status'
trustedfile = 'RAM:AOVPN-XD-TrustedIP.tmp'
ptpfile = 'DEVS:NetInterfaces/ptp0'
action = TRANSLATE(STRIP(action))

IF action = 'CHECK' THEN DO
    CALL ReadPeer
    peer = RESULT
    CALL ReadGateway
    physical = RESULT
    IF ~IsIPv4(peer) | peer = '0.0.0.0' THEN DO
        SAY '[CHECK] Invalid DESTINATION in' ptpfile
        EXIT 10
    END
    IF ~IsIPv4(physical) | physical = peer | physical = '0.0.0.0' THEN DO
        SAY '[CHECK] No valid physical default route; do not start VPN.'
        EXIT 10
    END
    IF EXISTS(state) THEN DO
        SAY '[CHECK] Old route snapshot exists:' state
        SAY '[CHECK] Inspect it and use DOWN before starting another session.'
        EXIT 10
    END
    SAY '[CHECK] Physical gateway:' physical 'VPN peer:' peer
    EXIT 0
END

IF action = 'UP' THEN DO
    IF EXISTS(state) THEN DO
        SAY '[ROUTES] Previous session state exists. Refusing to overwrite.'
        EXIT 10
    END
    CALL ReadPeer
    peer = RESULT
    CALL ReadGateway
    physical = RESULT
    CALL ReadTrusted
    trusted = RESULT
    IF ~IsIPv4(peer) | ~IsIPv4(physical) | ~IsIPv4(trusted) THEN DO
        SAY '[ROUTES] Invalid routing inputs (no routes changed):'
        SAY '[ROUTES] peer from ptp0 file:' peer
        SAY '[ROUTES] physical from ShowNetStatus:' physical
        SAY '[ROUTES] trusted_ip from hook GET:' trusted
        SAY '[ROUTES] Expected three numeric IPv4 addresses.'
        EXIT 10
    END
    IF peer = '0.0.0.0' | physical = '0.0.0.0' | trusted = '0.0.0.0' | physical = peer THEN DO
        SAY '[ROUTES] Invalid or already-tunneled default gateway.'
        EXIT 10
    END
    IF ~OPEN('save', state, 'WRITE') THEN DO
        SAY '[ROUTES] Cannot save original default route.'
        EXIT 10
    END
    CALL WRITELN('save', physical)
    CALL WRITELN('save', trusted)
    CALL WRITELN('save', peer)
    CALL CLOSE('save')
    SAY '[ROUTES] Original gateway:' physical 'server:' trusted 'peer:' peer

    /* Keep the outer OpenVPN UDP connection on the physical route. */
    'C:AddNetRoute HOSTDST=' || trusted || ' VIA=' || physical
    IF RC ~= 0 THEN DO
        SAY '[ROUTES] Could not pin VPN server; default route unchanged.'
        'C:Delete ' || state || ' QUIET'
        EXIT 10
    END

    'C:DeleteNetRoute DST=0.0.0.0'
    IF RC ~= 0 THEN DO
        SAY '[ROUTES] Could not remove physical default route.'
        SIGNAL Restore
    END

    'C:AddNetRoute DEFAULT=' || peer
    IF RC ~= 0 THEN DO
        SAY '[ROUTES] Could not install VPN default route.'
        SIGNAL Restore
    END
    CALL ReadGateway
    IF RESULT ~= peer THEN DO
        SAY '[ROUTES] VPN route did not become active.'
        SIGNAL Restore
    END
    SAY '[ROUTES] VPN default route active:' peer
    SAY '[ROUTES] NOTE: DNS settings have not been changed.'
    EXIT 0
END

IF action = 'DOWN' THEN DO
    IF ~EXISTS(state) THEN DO
        SAY '[ROUTES] No saved session; nothing to restore.'
        EXIT 0
    END
    IF ~OPEN('save', state, 'READ') THEN EXIT 10
    physical = STRIP(READLN('save'))
    trusted  = STRIP(READLN('save'))
    peer     = STRIP(READLN('save'))
    CALL CLOSE('save')
    IF ~IsIPv4(physical) | ~IsIPv4(peer) | ~IsIPv4(trusted) THEN DO
        SAY '[ROUTES] Invalid snapshot; leaving routes unchanged.'
        EXIT 10
    END
    SIGNAL Restore
END

SAY '[ROUTES] Usage: RX OPENVPN:AOVPN-Routes.rexx CHECK|UP|DOWN'
EXIT 10

Restore:
    /* This path is also the rollback for a failed UP. */
    CALL ReadGateway
    active = RESULT
    IF active = peer THEN DO
        SAY '[ROUTES] Removing VPN default route.'
        'C:DeleteNetRoute DST=0.0.0.0'
        IF RC ~= 0 THEN DO
            SAY '[ROUTES] VPN route removal failed; state preserved.'
            EXIT 10
        END
        active = ''
    END
    IF active = '' THEN DO
        SAY '[ROUTES] Restoring original default:' physical
        'C:AddNetRoute DEFAULT=' || physical
        IF RC ~= 0 THEN DO
            SAY '[ROUTES] RESTORE FAILED. State preserved. Manually run:'
            SAY 'C:AddNetRoute DEFAULT=' || physical
            EXIT 10
        END
    END
    ELSE IF active ~= physical THEN DO
        SAY '[ROUTES] Unknown active gateway:' active
        SAY '[ROUTES] Refusing to replace an unrelated route. State preserved.'
        EXIT 10
    END
    CALL ReadGateway
    IF RESULT ~= physical THEN DO
        SAY '[ROUTES] Original gateway did not become active. State preserved.'
        EXIT 10
    END
    'C:Delete ' || state || ' QUIET'
    SAY '[ROUTES] Original default restored:' physical
    SAY '[ROUTES] NOTE: Server host route may remain:' trusted
    EXIT 0

ReadGateway:
    ADDRESS COMMAND 'C:ShowNetStatus QUIET >' || status
    IF ~OPEN('shown', status, 'READ') THEN RETURN ''
    gateway = ''
    DO WHILE ~EOF('shown')
        row = STRIP(READLN('shown'))
        IF LEFT(TRANSLATE(row), 14) = 'DEFAULT ROUTE:' THEN DO
            PARSE VAR row . ':' value
            candidate = WORD(STRIP(value), 1)
            IF IsIPv4(candidate) THEN gateway = candidate
        END
    END
    CALL CLOSE('shown')
    RETURN gateway

ReadPeer:
    IF ~OPEN('cfg', ptpfile, 'READ') THEN RETURN ''
    peerip = ''
    DO WHILE ~EOF('cfg')
        row = STRIP(READLN('cfg'))
        IF row = '' | LEFT(row,1) = '#' | LEFT(row,1) = ';' THEN ITERATE
        PARSE VAR row key '=' value
        IF value = '' THEN PARSE VAR row key value
        key = TRANSLATE(STRIP(key))
        IF key = 'DESTINATION' | key = 'DESTINATIONADDR' | key = 'DESTINATIONADDRESS' THEN DO
            candidate = WORD(STRIP(value), 1)
            IF IsIPv4(candidate) THEN peerip = candidate
        END
    END
    CALL CLOSE('cfg')
    RETURN peerip

ReadTrusted:
    /* The route-up Shell captures local trusted_ip using internal GET.
     * Do not invoke C:Set: SET/GET are internal Shell commands. The
     * Rexx COMMAND host is another Shell, so read the hook's file instead
     * of relying on transitive LocalVar inheritance across RX. */
    IF ~OPEN('tv', trustedfile, 'READ') THEN RETURN ''
    trustedip = WORD(STRIP(READLN('tv')), 1)
    CALL CLOSE('tv')
    IF ~IsIPv4(trustedip) THEN RETURN ''
    RETURN trustedip

IsIPv4:
    PARSE ARG ip
    IF ip = '' | VERIFY(ip, '0123456789.') ~= 0 THEN RETURN 0
    PARSE VAR ip a '.' b '.' c '.' d
    IF a = '' | b = '' | c = '' | d = '' THEN RETURN 0
    IF LENGTH(a) > 3 | LENGTH(b) > 3 | LENGTH(c) > 3 | LENGTH(d) > 3 THEN RETURN 0
    IF VERIFY(a, '0123456789') ~= 0 | VERIFY(b, '0123456789') ~= 0 THEN RETURN 0
    IF VERIFY(c, '0123456789') ~= 0 | VERIFY(d, '0123456789') ~= 0 THEN RETURN 0
    IF a + 0 > 255 | b + 0 > 255 | c + 0 > 255 | d + 0 > 255 THEN RETURN 0
    RETURN 1
