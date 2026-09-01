-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_sigpass_pkg is
    type t_flag is record
        set   : std_logic;
        clear : std_logic;
    end record;

    -- A trigger every component drives, one element per instance, which is
    -- how a verification framework wires its instances together
    type t_trigs is array (natural range <>, natural range <>) of std_logic;
    signal global_trigs : t_trigs(0 to 1, 0 to 3) := (others => (others => '0'));

    procedure pulse (signal t : inout std_logic);
    procedure pass_on (signal t : inout std_logic);
    procedure set_flag (signal f : inout t_flag);
    procedure watch (signal w : in std_logic_vector; variable changed : out boolean);
end package;

package body t_vhdl_sigpass_pkg is
    procedure pulse (signal t : inout std_logic) is
    begin
        t <= '1';
    end procedure;

    -- What this one was handed, it hands on: the signal at the end of the
    -- chain is the one that gets a driver (4.2.2.3)
    procedure pass_on (signal t : inout std_logic) is
    begin
        pulse(t);
    end procedure;

    procedure set_flag (signal f : inout t_flag) is
    begin
        pass_on(f.set);
    end procedure;

    -- A signal parameter's history is the history of the signal the call
    -- passed, even where that is part of one (16.2.2)
    procedure watch (signal w : in std_logic_vector; variable changed : out boolean) is
        variable v_last : std_logic_vector(w'range) := w'last_value;
    begin
        changed := w'event and v_last /= w;
    end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_sigpass_pkg.all;

entity t_vhdl_sigpass_sub is
    generic (GC_INSTANCE : natural := 1;
             GC_W : natural := 4);
    port (iface : inout t_flag;
          data  : in std_logic_vector(GC_W - 1 downto 0));
end entity;

architecture test of t_vhdl_sigpass_sub is
    -- One element of a package's signal, at indices only elaboration
    -- settles, handed on from one call to the next
    alias my_trig : std_logic is global_trigs(0, GC_INSTANCE);
begin

    main : process is
    begin
        wait for 1 ns;
        set_flag(iface);
        pass_on(my_trig);
        wait;
    end process;

    seen : process (data) is
        variable v_changed : boolean;
    begin
        watch(data, v_changed);
        if v_changed then
            report "data changed to " & to_string(data);
        end if;
    end process;

end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_sigpass_pkg.all;

entity t_vhdl_sigpass is
end entity;

architecture test of t_vhdl_sigpass is
    signal iface : t_flag;
    signal data  : std_logic_vector(3 downto 0) := "0000";
begin

    dut : entity work.t_vhdl_sigpass_sub
        generic map (GC_INSTANCE => 1, GC_W => 4)
        port map (iface => iface, data => data);

    stim : process is
    begin
        wait for 2 ns;
        data <= "1010";
        wait;
    end process;

    check : process is
    begin
        wait for 3 ns;
        assert iface.set = '1' report "set" severity failure;
        assert global_trigs(0, 1) = '1' report "trig" severity failure;
        assert global_trigs(0, 0) = '0' report "other trig" severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
