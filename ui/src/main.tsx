// SPDX-FileCopyrightText: 2026 Thomas Christoffel
// SPDX-License-Identifier: Apache-2.0
import { mount, registerPage } from "../../espos/ui/src/mount";
import { PagesPage } from "./pages/pages";

registerPage({ path: "/pages", title: "Pages", page: PagesPage, order: 15 });
mount();
