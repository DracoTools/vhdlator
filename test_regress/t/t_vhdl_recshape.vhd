-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A record whose element carries its own bounds, given as an element of a
-- record aggregate what a call returns: the call's copy fixes the length,
-- so the record it returns has the other shape, and has to be converted as
-- an assignment converts it. Taken as it stood it came out as zero (GHDL's
-- issue646).
entity t_vhdl_recshape is
end entity;

architecture test of t_vhdl_recshape is
    type channel is record
        data : bit_vector;
        ack : bit;
    end record;

    type my_bus is record
        rd : channel;
        wr : channel;
    end record;

    function init_channel(width : natural; fill : bit) return channel is
    begin
        return (data => (width - 1 downto 0 => fill), ack => fill);
    end function;

    function init_bus(width : natural) return my_bus is
    begin
        return (rd => init_channel(width, '0'), wr => init_channel(width + 2, '1'));
    end function;

    constant b : my_bus := init_bus(12);
begin
    process
    begin
        assert b.rd.data'length = 12 report "rd length" severity failure;
        assert b.wr.data'length = 14 report "wr length" severity failure;
        assert b.rd.data = (11 downto 0 => '0') report "rd data" severity failure;
        assert b.wr.data = (13 downto 0 => '1') report "wr data" severity failure;
        assert b.rd.ack = '0' and b.wr.ack = '1' report "acks" severity failure;
        assert b.wr.data'left = 13 report "wr left" severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
