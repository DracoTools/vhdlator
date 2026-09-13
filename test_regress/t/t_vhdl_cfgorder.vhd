-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A configuration's binding names the entity it binds to, and no context
-- clause says so (3.4.2, 7.3.2).  This file is given first, so the entity
-- it names is only found if the binding puts it in the analysis order.

configuration cfg_fast of tb is
    for rtl
        for u : worker
            use entity work.worker(fast);
        end for;
    end for;
end configuration cfg_fast;
