/* AmiOpenVPN / AmiTCP-NG POC1: script-only routing helper. */
OPTIONS RESULTS
ADDRESS COMMAND
PARSE ARG action trusted vpn
state = 'RAM:AOVPN-Routes.state'
status = 'RAM:AOVPN-Routes.status'
action = TRANSLATE(STRIP(action))

IF action = 'UP' THEN DO
    IF trusted = '' | vpn = '' THEN DO
        SAY '[AOVPN-ROUTES] Missing trusted_ip or route_vpn_gateway.'
        EXIT 10
    END
    IF EXISTS(state) THEN DO
        SAY '[AOVPN-ROUTES] Previous state exists; use DOWN or inspect RAM:AOVPN-Routes.state.'
        EXIT 10
    END

    CALL ReadGateway
    physical = RESULT
    IF physical = '' | physical = vpn THEN DO
        SAY '[AOVPN-ROUTES] Cannot determine physical default gateway safely.'
        EXIT 10
    END
    SAY '[AOVPN-ROUTES] Physical gateway:' physical

    IF ~OPEN('saved', state, 'WRITE') THEN DO
        SAY '[AOVPN-ROUTES] Cannot save route state.'
        EXIT 10
    END
    CALL WRITELN('saved', physical)
    CALL WRITELN('saved', trusted)
    CALL WRITELN('saved', vpn)
    CALL CLOSE('saved')

    SAY '[AOVPN-ROUTES] Pin server endpoint via physical gateway.'
    'AMITCP:C/AddNetRoute HOSTDST=' || trusted || ' VIA=' || physical
    IF RC ~= 0 THEN DO
        SAY '[AOVPN-ROUTES] Host route failed; default route untouched.'
        EXIT 10
    END

    SAY '[AOVPN-ROUTES] Delete physical default route.'
    'AMITCP:C/DeleteNetRoute DST=0.0.0.0'
    IF RC ~= 0 THEN DO
        SAY '[AOVPN-ROUTES] Could not delete physical default route.'
        EXIT 10
    END

    SAY '[AOVPN-ROUTES] Install VPN default route.'
    'AMITCP:C/AddNetRoute DEFAULT=' || vpn
    IF RC ~= 0 THEN DO
        SAY '[AOVPN-ROUTES] VPN default route failed: restoring physical default.'
        'AMITCP:C/AddNetRoute DEFAULT=' || physical
        EXIT 10
    END
    SAY '[AOVPN-ROUTES] Default route now uses VPN peer' vpn
    SAY '[AOVPN-ROUTES] NOTE: host /32 route cannot be removed by NG DeleteNetRoute.'
    EXIT 0
END

IF action = 'DOWN' THEN DO
    IF ~OPEN('saved', state, 'READ') THEN DO
        SAY '[AOVPN-ROUTES] State file absent; no route changes made.'
        EXIT 10
    END
    physical = STRIP(READLN('saved'))
    trusted  = STRIP(READLN('saved'))
    vpn      = STRIP(READLN('saved'))
    CALL CLOSE('saved')
    IF physical = '' | vpn = '' THEN DO
        SAY '[AOVPN-ROUTES] Incomplete state; no route changes made.'
        EXIT 10
    END

    CALL ReadGateway
    active = RESULT
    IF active = vpn THEN DO
        SAY '[AOVPN-ROUTES] Remove VPN default route.'
        'AMITCP:C/DeleteNetRoute DST=0.0.0.0'
        IF RC ~= 0 THEN DO
            SAY '[AOVPN-ROUTES] Failed to remove VPN default route.'
            EXIT 10
        END
        SAY '[AOVPN-ROUTES] Restore physical default route.'
        'AMITCP:C/AddNetRoute DEFAULT=' || physical
        IF RC ~= 0 THEN DO
            SAY '[AOVPN-ROUTES] RESTORE FAILED: run C:AddNetRoute DEFAULT=' || physical
            EXIT 10
        END
    END
    ELSE IF active = physical THEN DO
        SAY '[AOVPN-ROUTES] Physical default route already active.'
    END
    ELSE DO
        SAY '[AOVPN-ROUTES] Unexpected active gateway:' active
        SAY '[AOVPN-ROUTES] Refusing to change an unknown default route.'
        EXIT 10
    END
    SAY '[AOVPN-ROUTES] Physical default route is restored.'
    SAY '[AOVPN-ROUTES] WARNING: server host /32 route remains:' trusted
    'C:Delete ' || state || ' QUIET'
    EXIT 0
END

SAY '[AOVPN-ROUTES] Usage: rx OPENVPN:AOVPN-Routes.rexx UP trusted_ip vpn_peer'
SAY '[AOVPN-ROUTES]    or: rx OPENVPN:AOVPN-Routes.rexx DOWN'
EXIT 10

ReadGateway:
    ADDRESS COMMAND 'AMITCP:C/ShowNetStatus >' || status
    IF RC ~= 0 THEN RETURN ''
    IF ~OPEN('shown', status, 'READ') THEN RETURN ''
    gw = ''
    row = READLN('shown')
    DO UNTIL EOF('shown')
        IF POS('Default gateway address', row) = 1 THEN DO
            PARSE VAR row . '=' gw
            gw = STRIP(gw)
        END
        row = READLN('shown')
    END
    CALL CLOSE('shown')
    IF gw = '' THEN RETURN ''
    IF VERIFY(gw, '0123456789.') ~= 0 THEN RETURN ''
    IF WORDS(gw) ~= 1 THEN RETURN ''
    RETURN gw
